/**
 * @file hk_qr.c
 * @brief ISO/IEC 18004 encoder, byte mode, versions 1-10.
 *
 * A wrong module here is invisible until someone points a phone at the screen
 * and nothing happens, so this follows the standard literally rather than
 * cleverly: the same block interleave, the same eight masks with the same
 * penalty scoring, the same BCH codes on the format and version fields. It is
 * verified module-by-module against an independent encoder rather than against
 * its own output, because an encoder that grades itself agrees with itself.
 *
 * Nothing allocates and nothing here touches ESP-IDF, which is what lets the
 * whole file run under a host test.
 */
#include "hk_qr.h"

#include <string.h>

_Static_assert(HK_QR_MAX_VERSION == 10,
               "the worst-case buffer sizes below are hand-computed for version 10");

/*
 * Worst cases over versions 1-10, so every buffer can live on the stack.
 * Version 10 holds 346 codewords in total; at error correction L it spends the
 * fewest of them on parity and so carries the most data, 274. No block in this
 * range carries more than 30 error correction codewords.
 */
#define HK_QR_MAX_CODEWORDS      346
#define HK_QR_MAX_DATA_CODEWORDS 274
#define HK_QR_MAX_ECC_PER_BLOCK  30

/* ISO/IEC 18004 tables 13-22, indexed [ecc level][version - 1]. */
static const uint8_t k_ecc_per_block[4][HK_QR_MAX_VERSION] = {
    {  7, 10, 15, 20, 26, 18, 20, 24, 30, 18 }, /* L */
    { 10, 16, 26, 18, 24, 16, 18, 22, 22, 26 }, /* M */
    { 13, 22, 18, 26, 18, 24, 18, 22, 20, 24 }, /* Q */
    { 17, 28, 22, 16, 22, 28, 26, 26, 24, 28 }, /* H */
};

static const uint8_t k_blocks[4][HK_QR_MAX_VERSION] = {
    { 1, 1, 1, 1, 1, 2, 2, 2, 2, 4 }, /* L */
    { 1, 1, 1, 2, 2, 4, 4, 4, 5, 5 }, /* M */
    { 1, 1, 2, 2, 4, 4, 6, 6, 8, 8 }, /* Q */
    { 1, 1, 2, 4, 4, 4, 5, 6, 8, 8 }, /* H */
};

/* The format field spells the levels in a different order than the enum does. */
static const uint8_t k_format_ecc[4] = { 1, 0, 3, 2 };

/* Penalty weights N1..N4 from clause 7.8.3. */
#define PENALTY_RUN      3
#define PENALTY_BLOCK    3
#define PENALTY_FINDER  40
#define PENALTY_BALANCE 10

/* ---------------------------------------------------------------- bitsets */

static bool bit_get(const uint8_t *bits, int index)
{
    return ((bits[index >> 3] >> (index & 7)) & 1u) != 0u;
}

static void bit_set(uint8_t *bits, int index, bool value)
{
    const uint8_t mask = (uint8_t)(1u << (index & 7));
    if (value) {
        bits[index >> 3] |= mask;
    } else {
        bits[index >> 3] &= (uint8_t)~mask;
    }
}

/* --------------------------------------------------------------- geometry */

/*
 * Modules available to data, before any of them are spent on codewords.
 * Deriving this beats a table: the same three corrections -- the alignment
 * patterns, their overlap with the timing rows, and the two version blocks
 * from version 7 up -- are the ones that make a hand-typed table wrong.
 */
static int raw_data_modules(int version)
{
    int result = (16 * version + 128) * version + 64;
    if (version >= 2) {
        const int align = version / 7 + 2;
        result -= (25 * align - 10) * align - 55;
        if (version >= 7) {
            result -= 36;
        }
    }
    return result;
}

static int total_codewords(int version)
{
    return raw_data_modules(version) / 8;
}

static int data_codewords(int version, int level)
{
    return total_codewords(version) -
           k_ecc_per_block[level][version - 1] * k_blocks[level][version - 1];
}

/**
 * Alignment pattern centres, low to high. Returns how many were written.
 *
 * The centres are evenly spaced with the *last* gap pinned to the far edge, so
 * the sequence is built backwards from `size - 7` and reversed.
 */
static int alignment_positions(int version, int *out)
{
    if (version == 1) {
        return 0;
    }
    const int count = version / 7 + 2;
    const int size  = 17 + 4 * version;
    const int step  = (version * 4 + count * 2 + 1) / (count * 2 - 2) * 2;
    int pos = size - 7;
    for (int i = count - 1; i >= 1; i--) {
        out[i] = pos;
        pos -= step;
    }
    out[0] = 6;
    return count;
}

/* --------------------------------------------------- Reed-Solomon, GF(256) */

static uint8_t gf_multiply(uint8_t x, uint8_t y)
{
    uint8_t z = 0;
    for (int i = 7; i >= 0; i--) {
        /* Double in the field: shift left, and fold the primitive polynomial
         * 0x11D back in whenever the shift pushed a bit out of the byte. */
        const bool overflow = (z & 0x80u) != 0u;
        z = (uint8_t)((unsigned)z << 1);
        if (overflow) {
            z ^= 0x1Du;
        }
        if (((y >> i) & 1u) != 0u) {
            z ^= x;
        }
    }
    return z;
}

/** Coefficients of the degree-`degree` generator, highest power omitted. */
static void rs_generator(int degree, uint8_t *out)
{
    memset(out, 0, (size_t)degree);
    out[degree - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; i++) {
        /* Multiply the running product by (x - root), in place. */
        for (int j = 0; j < degree; j++) {
            out[j] = gf_multiply(out[j], root);
            if (j + 1 < degree) {
                out[j] ^= out[j + 1];
            }
        }
        root = gf_multiply(root, 0x02);
    }
}

static void rs_remainder(const uint8_t *data, int length, const uint8_t *generator,
                         int degree, uint8_t *out)
{
    memset(out, 0, (size_t)degree);
    for (int i = 0; i < length; i++) {
        const uint8_t factor = (uint8_t)(data[i] ^ out[0]);
        memmove(out, out + 1, (size_t)(degree - 1));
        out[degree - 1] = 0;
        for (int j = 0; j < degree; j++) {
            out[j] ^= gf_multiply(generator[j], factor);
        }
    }
}

/* ----------------------------------------------------------------- canvas */

/*
 * `function` marks every module the standard owns -- finders, timing,
 * alignment, format, version. Masking and codeword placement both have to skip
 * exactly those, and a mask laid over a finder is a code no reader will find.
 */
typedef struct {
    int      size;
    uint8_t *modules;
    uint8_t  function[HK_QR_BUFFER_BYTES];
} canvas_t;

static bool get_module(const canvas_t *c, int x, int y)
{
    return bit_get(c->modules, y * c->size + x);
}

static void set_module(canvas_t *c, int x, int y, bool dark)
{
    bit_set(c->modules, y * c->size + x, dark);
}

static bool is_function(const canvas_t *c, int x, int y)
{
    return bit_get(c->function, y * c->size + x);
}

static void set_function_module(canvas_t *c, int x, int y, bool dark)
{
    set_module(c, x, y, dark);
    bit_set(c->function, y * c->size + x, true);
}

/**
 * Finders are drawn centred on a 9x9: the 7x7 pattern plus the separator ring
 * around it. All three sit in corners, so for each one a row and a column of
 * that ring land outside the symbol. Skipping them is not tidiness -- the
 * bitset has no per-row padding, so x == -1 would write the byte before the
 * buffer and x == size would silently darken the first module of the next row.
 */
static void draw_finder(canvas_t *c, int cx, int cy)
{
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            const int ax   = dx < 0 ? -dx : dx;
            const int ay   = dy < 0 ? -dy : dy;
            const int ring = ax > ay ? ax : ay;
            const int x    = cx + dx;
            const int y    = cy + dy;
            if (x >= 0 && x < c->size && y >= 0 && y < c->size) {
                set_function_module(c, x, y, ring != 2 && ring != 4);
            }
        }
    }
}

static void draw_alignment(canvas_t *c, int cx, int cy)
{
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            const int ax   = dx < 0 ? -dx : dx;
            const int ay   = dy < 0 ? -dy : dy;
            const int ring = ax > ay ? ax : ay;
            set_function_module(c, cx + dx, cy + dy, ring != 1);
        }
    }
}

/**
 * The 15-bit format field: five bits of level and mask, ten of BCH(15,5)
 * parity, then a fixed XOR so an all-zero field cannot look valid.
 */
static void draw_format(canvas_t *c, int level, int mask)
{
    const unsigned value = (unsigned)(k_format_ecc[level] << 3) | (unsigned)mask;
    unsigned rem = value;
    for (int i = 0; i < 10; i++) {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537u);
    }
    const unsigned bits = ((value << 10) | (rem & 0x3FFu)) ^ 0x5412u;

    /* Written twice, so a damaged corner still leaves one readable copy. */
    for (int i = 0; i <= 5; i++) {
        set_function_module(c, 8, i, ((bits >> i) & 1u) != 0u);
    }
    set_function_module(c, 8, 7, ((bits >> 6) & 1u) != 0u);
    set_function_module(c, 8, 8, ((bits >> 7) & 1u) != 0u);
    set_function_module(c, 7, 8, ((bits >> 8) & 1u) != 0u);
    for (int i = 9; i < 15; i++) {
        set_function_module(c, 14 - i, 8, ((bits >> i) & 1u) != 0u);
    }
    for (int i = 0; i < 8; i++) {
        set_function_module(c, c->size - 1 - i, 8, ((bits >> i) & 1u) != 0u);
    }
    for (int i = 8; i < 15; i++) {
        set_function_module(c, 8, c->size - 15 + i, ((bits >> i) & 1u) != 0u);
    }
    set_function_module(c, 8, c->size - 8, true); /* always dark, by definition */
}

/** Version 7 and up carry their own number, BCH(18,6) coded, twice. */
static void draw_version(canvas_t *c, int version)
{
    if (version < 7) {
        return;
    }
    unsigned rem = (unsigned)version;
    for (int i = 0; i < 12; i++) {
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25u);
    }
    const unsigned bits = ((unsigned)version << 12) | (rem & 0xFFFu);

    for (int i = 0; i < 18; i++) {
        const bool bit = ((bits >> i) & 1u) != 0u;
        const int  a   = c->size - 11 + i % 3;
        const int  b   = i / 3;
        set_function_module(c, a, b, bit);
        set_function_module(c, b, a, bit);
    }
}

static void draw_function_patterns(canvas_t *c, int version, int level)
{
    for (int i = 0; i < c->size; i++) {
        set_function_module(c, 6, i, i % 2 == 0);
        set_function_module(c, i, 6, i % 2 == 0);
    }
    draw_finder(c, 3, 3);
    draw_finder(c, c->size - 4, 3);
    draw_finder(c, 3, c->size - 4);

    int align[HK_QR_MAX_VERSION / 7 + 2];
    const int count = alignment_positions(version, align);
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < count; j++) {
            /* Three centres sit inside a finder and are simply not drawn. */
            const bool corner = (i == 0 && j == 0) ||
                                (i == 0 && j == count - 1) ||
                                (i == count - 1 && j == 0);
            if (!corner) {
                draw_alignment(c, align[i], align[j]);
            }
        }
    }

    /* Mask 0 is a placeholder; the real one is written once it is chosen. What
     * matters now is that these modules are reserved before data is placed. */
    draw_format(c, level, 0);
    draw_version(c, version);
}

/**
 * Lay the codewords into the zigzag, two columns at a time, bottom-right first.
 *
 * Any modules left over once the codewords run out are the remainder bits: the
 * standard leaves them light, and they are still masked like any other data.
 */
static void draw_codewords(canvas_t *c, const uint8_t *codewords, int count)
{
    const int bits = count * 8;
    int i = 0;
    for (int right = c->size - 1; right >= 1; right -= 2) {
        if (right == 6) {
            right = 5; /* the vertical timing column is not part of the zigzag */
        }
        for (int vert = 0; vert < c->size; vert++) {
            for (int j = 0; j < 2; j++) {
                const int  x      = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int  y      = upward ? c->size - 1 - vert : vert;
                if (!is_function(c, x, y) && i < bits) {
                    set_module(c, x, y,
                               ((codewords[i >> 3] >> (7 - (i & 7))) & 1u) != 0u);
                    i++;
                }
            }
        }
    }
}

/** XOR in place, so calling it twice with the same mask undoes it. */
static void apply_mask(canvas_t *c, int mask)
{
    for (int y = 0; y < c->size; y++) {
        for (int x = 0; x < c->size; x++) {
            bool invert;
            switch (mask) {
            case 0:  invert = (x + y) % 2 == 0;                       break;
            case 1:  invert = y % 2 == 0;                             break;
            case 2:  invert = x % 3 == 0;                             break;
            case 3:  invert = (x + y) % 3 == 0;                       break;
            case 4:  invert = (x / 3 + y / 2) % 2 == 0;               break;
            case 5:  invert = x * y % 2 + x * y % 3 == 0;             break;
            case 6:  invert = (x * y % 2 + x * y % 3) % 2 == 0;       break;
            case 7:  invert = ((x + y) % 2 + x * y % 3) % 2 == 0;     break;
            default: invert = false;                                  break;
            }
            if (invert && !is_function(c, x, y)) {
                set_module(c, x, y, !get_module(c, x, y));
            }
        }
    }
}

/* ---------------------------------------------------------------- penalty */

/*
 * Rule 3 looks for the finder's 1:1:3:1:1 ratio with four modules of light on
 * one side of it. The seven most recent run lengths are enough to see that,
 * and the quiet zone counts as light, which is why the first run of a line is
 * lengthened by a full line width.
 */
static void finder_add_history(int run, int *history, int size)
{
    if (history[0] == 0) {
        run += size;
    }
    for (int i = 6; i > 0; i--) {
        history[i] = history[i - 1];
    }
    history[0] = run;
}

static int finder_count(const int *history)
{
    const int  n    = history[1];
    const bool core = n > 0 && history[2] == n && history[3] == n * 3 &&
                      history[4] == n && history[5] == n;
    return (core && history[0] >= n * 4 && history[6] >= n ? 1 : 0) +
           (core && history[6] >= n * 4 && history[0] >= n ? 1 : 0);
}

/** Close the line: flush the final run, then the quiet zone beyond it. */
static int finder_terminate(bool run_dark, int run, int *history, int size)
{
    if (run_dark) {
        finder_add_history(run, history, size);
        run = 0;
    }
    run += size;
    finder_add_history(run, history, size);
    return finder_count(history);
}

static long penalty(const canvas_t *c)
{
    const int size = c->size;
    long      score = 0;

    for (int y = 0; y < size; y++) {
        int  history[7] = { 0 };
        bool run_dark   = false;
        int  run        = 0;
        for (int x = 0; x < size; x++) {
            const bool dark = get_module(c, x, y);
            if (dark == run_dark) {
                run++;
                if (run == 5) {
                    score += PENALTY_RUN;
                } else if (run > 5) {
                    score++;
                }
            } else {
                finder_add_history(run, history, size);
                if (!run_dark) {
                    score += finder_count(history) * PENALTY_FINDER;
                }
                run_dark = dark;
                run      = 1;
            }
        }
        score += finder_terminate(run_dark, run, history, size) * PENALTY_FINDER;
    }
    for (int x = 0; x < size; x++) {
        int  history[7] = { 0 };
        bool run_dark   = false;
        int  run        = 0;
        for (int y = 0; y < size; y++) {
            const bool dark = get_module(c, x, y);
            if (dark == run_dark) {
                run++;
                if (run == 5) {
                    score += PENALTY_RUN;
                } else if (run > 5) {
                    score++;
                }
            } else {
                finder_add_history(run, history, size);
                if (!run_dark) {
                    score += finder_count(history) * PENALTY_FINDER;
                }
                run_dark = dark;
                run      = 1;
            }
        }
        score += finder_terminate(run_dark, run, history, size) * PENALTY_FINDER;
    }

    for (int y = 0; y < size - 1; y++) {
        for (int x = 0; x < size - 1; x++) {
            const bool dark = get_module(c, x, y);
            if (dark == get_module(c, x + 1, y) &&
                dark == get_module(c, x, y + 1) &&
                dark == get_module(c, x + 1, y + 1)) {
                score += PENALTY_BLOCK;
            }
        }
    }

    int dark = 0;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (get_module(c, x, y)) {
                dark++;
            }
        }
    }
    /* Rule 4 charges for every 5% the dark share strays from half. */
    const int total = size * size;
    const int diff  = dark * 20 - total * 10;
    const int away  = diff < 0 ? -diff : diff;
    score += (long)((away + total - 1) / total - 1) * PENALTY_BALANCE;

    return score;
}

/* ------------------------------------------------------------------- bits */

static void append_bits(uint8_t *buf, int *cursor, uint32_t value, int count)
{
    for (int i = count - 1; i >= 0; i--) {
        const int index = (*cursor)++;
        if (((value >> i) & 1u) != 0u) {
            buf[index >> 3] |= (uint8_t)(0x80u >> (index & 7));
        }
    }
}

/* ------------------------------------------------------------------- API */

bool hk_qr_encode_bytes(const uint8_t *data, size_t length, hk_qr_ecc_t ecc,
                        hk_qr_t *out)
{
    const int level = (int)ecc;
    if (out == NULL || (data == NULL && length > 0) || level < 0 || level > 3) {
        return false;
    }

    /*
     * Smallest version that fits. The character count field widens from 8 to
     * 16 bits at version 10, so the fit has to be re-asked at every version
     * rather than measured once against the largest.
     */
    int version    = 0;
    int count_bits = 0;
    int capacity   = 0;
    for (int v = 1; v <= HK_QR_MAX_VERSION; v++) {
        count_bits = v < 10 ? 8 : 16;
        capacity   = data_codewords(v, level) * 8;
        if (length <= (size_t)((capacity - 4 - count_bits) / 8)) {
            version = v;
            break;
        }
    }
    if (version == 0) {
        return false; /* the caller gets a refusal, never a truncated payload */
    }

    uint8_t payload[HK_QR_MAX_DATA_CODEWORDS];
    memset(payload, 0, sizeof payload);
    int cursor = 0;
    append_bits(payload, &cursor, 0x4u, 4); /* byte mode */
    append_bits(payload, &cursor, (uint32_t)length, count_bits);
    for (size_t i = 0; i < length; i++) {
        append_bits(payload, &cursor, data[i], 8);
    }
    /* Terminator, then zeros to the byte boundary, then the two pad codewords
     * the standard names, alternating until the capacity is exactly full. */
    append_bits(payload, &cursor, 0, capacity - cursor < 4 ? capacity - cursor : 4);
    append_bits(payload, &cursor, 0, (8 - cursor % 8) % 8);
    for (uint32_t pad = 0xECu; cursor < capacity; pad ^= 0xECu ^ 0x11u) {
        append_bits(payload, &cursor, pad, 8);
    }

    const int n_data   = capacity / 8;
    const int n_total  = total_codewords(version);
    const int n_blocks = k_blocks[level][version - 1];
    const int ecc_len  = k_ecc_per_block[level][version - 1];
    /* Blocks differ by at most one codeword; the short ones come first. */
    const int short_len = n_data / n_blocks;
    const int n_short   = n_blocks - n_data % n_blocks;

    uint8_t generator[HK_QR_MAX_ECC_PER_BLOCK];
    uint8_t parity[HK_QR_MAX_ECC_PER_BLOCK];
    uint8_t codewords[HK_QR_MAX_CODEWORDS];
    rs_generator(ecc_len, generator);
    memset(codewords, 0, sizeof codewords);

    /*
     * Interleave as each block is computed, so no second copy of the blocks is
     * needed. Data codeword k of block b goes to column k, except in the last
     * data column, which the short blocks never reach and which therefore
     * closes up by exactly the number of short blocks.
     */
    int offset = 0;
    for (int b = 0; b < n_blocks; b++) {
        const int len = short_len + (b < n_short ? 0 : 1);
        rs_remainder(payload + offset, len, generator, ecc_len, parity);
        for (int k = 0; k < len; k++) {
            int index = k * n_blocks + b;
            if (k == short_len) {
                index -= n_short;
            }
            codewords[index] = payload[offset + k];
        }
        for (int k = 0; k < ecc_len; k++) {
            codewords[n_data + k * n_blocks + b] = parity[k];
        }
        offset += len;
    }

    canvas_t canvas;
    canvas.size    = 17 + 4 * version;
    canvas.modules = out->modules;
    memset(canvas.modules, 0, sizeof out->modules);
    memset(canvas.function, 0, sizeof canvas.function);

    draw_function_patterns(&canvas, version, level);
    draw_codewords(&canvas, codewords, n_total);

    /*
     * All eight masks are tried and scored. This is not a formality: the mask
     * is what keeps a large light or dark field from looking like a finder to
     * the reader, and the standard's own tie-break is lowest penalty, lowest
     * mask number.
     */
    int  best       = 0;
    long best_score = 0;
    for (int mask = 0; mask < 8; mask++) {
        draw_format(&canvas, level, mask);
        apply_mask(&canvas, mask);
        const long score = penalty(&canvas);
        if (mask == 0 || score < best_score) {
            best       = mask;
            best_score = score;
        }
        apply_mask(&canvas, mask);
    }
    draw_format(&canvas, level, best);
    apply_mask(&canvas, best);

    out->size = (uint8_t)canvas.size;
    return true;
}

bool hk_qr_encode(const char *text, hk_qr_ecc_t ecc, hk_qr_t *out)
{
    if (text == NULL) {
        return false;
    }
    return hk_qr_encode_bytes((const uint8_t *)text, strlen(text), ecc, out);
}

bool hk_qr_module(const hk_qr_t *qr, int x, int y)
{
    /* Out of range reads light, so a caller drawing a quiet zone can simply
     * walk past the edge instead of special-casing it. */
    if (qr == NULL || x < 0 || y < 0 || x >= (int)qr->size || y >= (int)qr->size) {
        return false;
    }
    return bit_get(qr->modules, y * (int)qr->size + x);
}
