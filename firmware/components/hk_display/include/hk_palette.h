/**
 * @file hk_palette.h
 * @brief The one place a colour is decided.
 *
 * These are the design's own tokens, converted once to RGB565. Two reasons they
 * live in a header rather than at each call site: a violet chosen twice is two
 * violets, and RGB565 quantises unevenly -- green keeps six bits and the others
 * five -- so a value picked by eye in one file will not match the same value
 * picked by eye in another.
 *
 * The palette is warm-violet on near-black. Nothing here is a pure grey: a
 * neutral next to this much violet reads as a mistake, so the greys carry the
 * accent's hue at low saturation. That is why the label colours below have more
 * blue than red in them and none of them is #808080.
 */
#ifndef HK_PALETTE_H
#define HK_PALETTE_H

#include "hk_draw.h"

/* RGB565 at compile time. hk_rgb() is the runtime form of the same thing. */
#define HK_RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/* --- ground --------------------------------------------------------- */
#define HK_C_SHELL     HK_RGB565(0x05, 0x05, 0x0a)  /* outside the vignette   */
#define HK_C_VOID      HK_RGB565(0x07, 0x09, 0x14)  /* deep space             */
#define HK_C_DEEP      HK_RGB565(0x13, 0x1a, 0x3c)  /* the disc's outer haze  */

/* --- the galaxy ----------------------------------------------------- */
#define HK_C_NEBULA    HK_RGB565(0x46, 0x2c, 0x7e)  /* arm gas                */
#define HK_C_DUST      HK_RGB565(0xb1, 0x62, 0x8c)  /* HII regions            */
#define HK_C_CORE      HK_RGB565(0xf7, 0xcf, 0x86)  /* the bulge, and warmth  */
#define HK_C_STARLIGHT HK_RGB565(0xee, 0xf2, 0xff)  /* young stars            */

/* --- foreground ----------------------------------------------------- */
#define HK_C_ACCENT    HK_RGB565(0xb9, 0x8b, 0xf0)  /* the violet that leads  */
#define HK_C_INK       HK_RGB565(0xec, 0xea, 0xf3)  /* primary text           */
#define HK_C_INK_2     HK_RGB565(0xa5, 0xa1, 0xb4)  /* secondary text         */
#define HK_C_INK_3     HK_RGB565(0x86, 0x82, 0x8f)  /* labels, chips          */
#define HK_C_GLASS     HK_RGB565(0x1b, 0x1a, 0x24)  /* chip and frame fill    */
#define HK_C_RULE      HK_RGB565(0x26, 0x24, 0x37)  /* hairlines              */

/* --- semantic ------------------------------------------------------- */
#define HK_C_EMBER     HK_RGB565(0xe8, 0x55, 0x2d)  /* warning                */
#define HK_C_ALARM     HK_RGB565(0xff, 0x45, 0x3a)  /* fault, factory reset   */
#define HK_C_GOOD      HK_RGB565(0x3d, 0xd6, 0x8c)  /* ready, charged         */
#define HK_C_WAIT      HK_RGB565(0xff, 0xd1, 0x66)  /* connecting, holding    */

/* Chip and frame fills are drawn at an alpha, not as a solid colour: the
 * galaxy has to stay visible through them or the screen becomes a card on a
 * picture rather than one surface. */
#define HK_A_GLASS  86
#define HK_A_FRAME  120
#define HK_A_RULE   150

#endif /* HK_PALETTE_H */
