#!/usr/bin/env python3
"""Rasterise Inter into hk_font_data.c.

    Typeface : Inter, version 4.001 (git-66647c0bb), by Rasmus Andersson
    Source   : https://github.com/rsms/inter -- the variable `Inter.ttf`
    SHA-256  : 29160a80ff49ddcab2c97711247e08b1fab27a484a329ce8b813d820dc559031
    Licence  : SIL Open Font License 1.1, copied verbatim into Inter-OFL.txt
               beside this file. The OFL permits embedding the rasterised
               outlines in this firmware; it forbids selling the font itself
               and requires the notice to travel with it, which is why the
               licence is in the repository and the .ttf is not.

The .ttf is deliberately NOT committed. Point --font at a copy:

    python3 tools/gen_font.py --font ~/Downloads/Inter.ttf \\
        --out hk_font_data.c

Needs Pillow (developed against 12.3). Nothing else, and nothing at build
time: the output is a C file that is compiled like any other.


WHY THESE THREE SIZES
---------------------
The panel is a 1.28 in round GC9A01: 240x240 px across roughly 32.4 mm of
glass, so one pixel is 0.135 mm and the part runs at about 188 ppi. Content
stays inside HK_DRAW_SAFE_R = 104 px, which leaves 208 px across the middle --
that number, not a typographic scale, is what fixes the largest face.

    display  19 px / wght 600
        "Harman Kardom 932C" sets 205 px at this size and 215 px at 20 px, so
        19 is the largest size at which the device's own name fits the safe
        width on one line. Everything longer than the name -- a track title --
        needs the marquee anyway, which is what hk_font_draw_faded() is for.
        Weight 600 rather than 500 because this face is the one drawn over the
        galaxy: at 19 px its stems measure 2.45 px of ink against 2.06 px at
        500, and a 2 px stem is the point where a light glyph stops greying out
        over a bright background.

    body     14 px / wght 500
        The artist line and the numbers. Two of these stack in 36 px
        (ascent 14 + descent 4, twice), which is what the playing screen has
        between the cover art and the progress ring.

    small    11 px / wght 500
        Chips and labels, and the reason there is no fourth face below it. Its
        x-height is 6 px, which at arm's length subtends 5.6 arcmin -- about
        where high-contrast acuity gives out. The hard floor is 9 px: there the
        x-height falls to 5 and Inter's 'a' loses its counter to a single dim
        pixel. Between 10 and 11 there is nothing to choose on legibility (both
        have a 6 px x-height); 11 is taken so the three faces step 11 / 14 / 19,
        near enough to a constant ratio that the hierarchy reads as a scale
        rather than as three sizes somebody happened to pick. Weight 500 rather
        than 400 because at 11 px a Regular stem measures 0.99 px of ink, and a
        stem that rounds to one column of 4-bit coverage reads as grey rather
        than as a letter.

Optical size is pinned to 14, the bottom of Inter's opsz axis, for all three.
At these pixel sizes the axis changes no advance we measured ("Harman Kardom
932C" sets 205 px at both opsz 14 and opsz 19), so this is a choice about which
end of the design space to sit in rather than about fit -- and nothing on a
32 mm disc is optically a display size, whatever its pixel count.

None of this makes the panel readable from a couple of metres, and choosing
larger faces would not either. At 2 m a pixel subtends 0.23 arcmin, so the
display face's 11 px x-height is 2.6 arcmin -- under the ~5 arcmin that
high-contrast acuity needs. What the sizes above are chosen for is: the shape
of a word you already know is recognisable across a room, and all three faces
are comfortably readable at arm's length (at 0.5 m the x-heights are 5.6, 7.4
and 10.2 arcmin).


OUTPUT FORMAT
-------------
Coverage is 4 bits per pixel, two pixels per byte, HIGH NIBBLE FIRST, each row
padded to a whole byte -- the shape hk_gfx_blit_a4() consumes. Masks are
trimmed to their ink and identical masks are shared, so a space costs its index
entry and no coverage at all, and `left`/`top` carry the trim back.

`top` is the baseline-relative y of the mask's top edge in screen coordinates,
where y grows downward. It is therefore NEGATIVE for the part of a glyph above
the baseline, and the draw call is simply `blit(x + left, y + top)` with no
sign to remember.

The codepoint index is sorted ascending so hk_font.c can binary search it. That
is a property this generator guarantees, not one the C side rechecks.
"""

import argparse
import sys
import unicodedata

from PIL import Image, ImageFont

# --- what the panel has to be able to say ------------------------------------
#
# ASCII, then the Latin-1 letters, then the six Turkish letters that Latin-1
# does not carry. Turkish is the first language of this project and AirPlay
# hands us UTF-8, so 'ğ' is an ordinary letter here and not an exotic.
#
# The punctuation at the end is the part that is easy to leave out and expensive
# to leave out. Apple Music metadata is typeset, not typed: it uses U+2019 for
# an apostrophe, curly quotes, and both dashes. A missing glyph draws a space,
# so omitting these would silently punch holes in real track titles -- and the
# em dash in particular is what separates a title from its performer.

CODEPOINTS = sorted(set(
    list(range(0x20, 0x7F)) +                                   # ASCII
    [cp for cp in range(0xC0, 0x100) if cp not in (0xD7, 0xF7)] +  # Latin-1 letters
    [0x011E, 0x011F, 0x0130, 0x0131, 0x015E, 0x015F] +          # Ğ ğ İ ı Ş ş
    [0x00B0, 0x00B7] +                                          # degree, middot
    [0x2013, 0x2014] +                                          # en dash, em dash
    [0x2018, 0x2019, 0x201C, 0x201D] +                          # typeset quotes
    [0x2026]                                                    # ellipsis
))

# name, pixel size, weight axis, optical size axis
FACES = [
    ("small", 11, 500, 14),
    ("body", 14, 500, 14),
    ("display", 19, 600, 14),
]

# A codepoint in the Private Use Area that no text font maps. Whatever Pillow
# renders for it is this font's .notdef, and any glyph that renders identically
# is one the font does not actually have -- which is worth failing on, because
# the alternative is shipping a table full of confident-looking blanks.
PROBE = 0xE000


def render(font, ch):
    """Return (advance, trimmed 2D coverage 0..255, left, top_from_line_origin)."""
    advance = int(round(font.getlength(ch)))
    mask, offset = font.getmask2(ch, mode="L")
    w, h = mask.size
    if w == 0 or h == 0:
        return advance, [], 0, 0
    pixels = list(mask)

    rows = [pixels[r * w:(r + 1) * w] for r in range(h)]

    # Trim to ink. Pillow's mask carries the glyph's side bearings and, for
    # accented capitals, a lot of empty space above the letter; every column we
    # keep costs half a byte per row in flash for the life of the product.
    top = 0
    while top < h and not any(rows[top]):
        top += 1
    if top == h:
        return advance, [], 0, 0
    bottom = h
    while not any(rows[bottom - 1]):
        bottom -= 1
    left = 0
    while not any(row[left] for row in rows[top:bottom]):
        left += 1
    right = w
    while not any(row[right - 1] for row in rows[top:bottom]):
        right -= 1

    ink = [row[left:right] for row in rows[top:bottom]]
    return advance, ink, offset[0] + left, offset[1] + top


def pack4(ink):
    """4 bits per pixel, two per byte, high nibble first, rows byte-aligned."""
    out = bytearray()
    for row in ink:
        nibbles = [(v * 15 + 127) // 255 for v in row]
        if len(nibbles) % 2:
            nibbles.append(0)          # pad the row out to a whole byte
        for i in range(0, len(nibbles), 2):
            out.append((nibbles[i] << 4) | nibbles[i + 1])
    return bytes(out)


def build_face(path, name, size, weight, opsz):
    font = ImageFont.truetype(path, size)
    font.set_variation_by_axes([opsz, weight])
    ascent, descent = font.getmetrics()

    notdef = render(font, chr(PROBE))

    blob = bytearray()
    # Identical masks share one blob entry. Mostly this catches the blanks, but
    # at 11 px it also catches genuine collisions like 'İ' against 'Í'.
    seen = {}
    glyphs = []
    missing = []

    for cp in CODEPOINTS:
        ch = chr(cp)
        advance, ink, left, top_line = render(font, ch)
        if ink and (advance, ink) == (notdef[0], notdef[1]):
            missing.append(cp)
            continue

        data = pack4(ink)
        height = len(ink)
        width = len(ink[0]) if ink else 0
        if data:
            if data in seen:
                offset = seen[data]
            else:
                offset = len(blob)
                seen[data] = offset
                blob.extend(data)
        else:
            offset = 0

        # A glyph with no ink has no position either. Emitting the bearings a
        # space happens to have measured would put numbers in the table that
        # nothing reads and that look, to the next person, like they matter.
        top = top_line - ascent if data else 0   # baseline-relative, negative above
        if not data:
            left = 0
        for field, value, lo, hi in (
            ("offset", offset, 0, 0xFFFF),
            ("width", width, 0, 0xFF),
            ("height", height, 0, 0xFF),
            ("left", left, -128, 127),
            ("top", top, -128, 127),
            ("advance", advance, 0, 0xFF),
        ):
            if not lo <= value <= hi:
                raise SystemExit(
                    f"{name}: U+{cp:04X} {field}={value} does not fit "
                    f"hk_glyph_t ({lo}..{hi}). The face is too large for the "
                    f"frozen table format."
                )
        glyphs.append((cp, offset, width, height, left, top, advance))

    if missing:
        raise SystemExit(
            f"{name}: {path} has no glyph for "
            + ", ".join(f"U+{cp:04X}" for cp in missing)
        )

    return {
        "name": name,
        "size": size,
        "weight": weight,
        "opsz": opsz,
        "ascent": ascent,
        "descent": descent,
        "glyphs": glyphs,
        "blob": bytes(blob),
    }


def emit_blob(out, ident, blob):
    out.append(f"static const uint8_t {ident}[] = {{")
    for i in range(0, len(blob), 16):
        row = ", ".join(f"0x{b:02X}" for b in blob[i:i + 16])
        out.append(f"    {row},")
    out.append("};")
    out.append("")


def emit_face(out, face):
    ident = face["name"]
    emit_blob(out, f"{ident}_coverage", face["blob"])

    out.append(f"static const uint16_t {ident}_codepoints[] = {{")
    cps = [g[0] for g in face["glyphs"]]
    for i in range(0, len(cps), 12):
        out.append("    " + ", ".join(f"0x{c:04X}" for c in cps[i:i + 12]) + ",")
    out.append("};")
    out.append("")

    out.append(f"static const hk_glyph_t {ident}_glyphs[] = {{")
    for cp, offset, w, h, left, top, advance in face["glyphs"]:
        label = unicodedata.name(chr(cp), f"U+{cp:04X}")
        out.append(
            f"    {{ {offset:5d}, {w:3d}, {h:3d}, {left:4d}, {top:4d}, "
            f"{advance:3d} }},   /* U+{cp:04X} {label} */"
        )
    out.append("};")
    out.append("")

    line_height = face["ascent"] + face["descent"]
    out.append(f"static const hk_face_t {ident}_face = {{")
    out.append(f'    .name        = "{ident}-{face["size"]}-{face["weight"]}",')
    out.append(f'    .ascent      = {face["ascent"]},')
    out.append(f'    .descent     = {face["descent"]},')
    out.append(f"    .line_height = {line_height},")
    out.append(f"    .first       = 0x{face['glyphs'][0][0]:04X},")
    out.append(f"    .count       = {len(face['glyphs'])},")
    out.append(f"    .codepoints  = {ident}_codepoints,")
    out.append(f"    .glyphs      = {ident}_glyphs,")
    out.append(f"    .coverage    = {ident}_coverage,")
    out.append("};")
    out.append("")


def cost(face):
    """Bytes of flash this face occupies, as the compiler will lay it out."""
    n = len(face["glyphs"])
    return len(face["blob"]) + 2 * n + 8 * n   # sizeof(hk_glyph_t) is 8


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--font", default="Inter.ttf", help="path to Inter.ttf")
    ap.add_argument("--out", default="hk_font_data.c")
    ap.add_argument("--proof", help="also write a PNG proof sheet here")
    args = ap.parse_args()

    faces = [build_face(args.font, *f) for f in FACES]
    total = sum(cost(f) for f in faces)

    out = [
        "/*",
        " * GENERATED by tools/gen_font.py -- do not edit.",
        " *",
        " * Typeface: Inter 4.001 (git-66647c0bb) by Rasmus Andersson,",
        " * https://github.com/rsms/inter, SIL Open Font License 1.1. The licence",
        " * text is in tools/Inter-OFL.txt and travels with these outlines.",
        " *",
        " * Faces, and why they are these sizes, are documented in the generator.",
        f" * The three tables below cost {total} bytes of flash between them.",
        " *",
        " * Regenerate rather than patching a nibble here:",
        " *",
        " *     python3 tools/gen_font.py --font /path/to/Inter.ttf",
        " */",
        "",
        '#include "hk_font.h"',
        "",
    ]
    for face in faces:
        out.append(
            f"/* {face['name']}: {face['size']} px, weight {face['weight']}, "
            f"opsz {face['opsz']}, {len(face['glyphs'])} glyphs, "
            f"{cost(face)} bytes of flash. */"
        )
        emit_face(out, face)

    out.append("/*")
    out.append(" * The accessors live here rather than in hk_font.c so that the engine")
    out.append(" * never names a table: regenerating this file cannot break that one.")
    out.append(" */")
    for face, fn in zip(faces, ("hk_font_small", "hk_font_body", "hk_font_display")):
        out.append(f"const hk_face_t *{fn}(void) {{ return &{face['name']}_face; }}")
    out.append("")

    with open(args.out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(out) + "\n")

    for face in faces:
        print(
            f"{face['name']:8s} {face['size']:2d}px/{face['weight']} "
            f"asc {face['ascent']:2d} desc {face['descent']} "
            f"{len(face['glyphs']):3d} glyphs  "
            f"coverage {len(face['blob']):5d} B  total {cost(face):5d} B",
            file=sys.stderr,
        )
    print(f"{'total':8s} {total} bytes -> {args.out}", file=sys.stderr)

    if args.proof:
        write_proof(args.font, args.proof)


def write_proof(path, dest):
    """A Pillow-drawn reference, to compare the C renderer against."""
    lines = [
        "Nihavent Longa — Kemani Tatyos Efendi",
        "Şarkı çalıyor · 1938",
        "Harman Kardom 932C",
        "0123456789 %",
    ]
    img = Image.new("L", (240, 240), 0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(img)
    y = 8
    for name, size, weight, opsz in FACES:
        font = ImageFont.truetype(path, size)
        font.set_variation_by_axes([opsz, weight])
        ascent, descent = font.getmetrics()
        for line in lines:
            d.text((4, y + ascent), line, font=font, fill=255, anchor="ls")
            y += ascent + descent
        y += 2
    img.save(dest)
    print(f"proof sheet -> {dest}", file=sys.stderr)


if __name__ == "__main__":
    main()
