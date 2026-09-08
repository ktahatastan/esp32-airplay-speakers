#!/usr/bin/env python3
"""Rasterise the icon set into hk_icons_data.c.

The shapes come from the design's own SVG symbols (hk-screen.html, the <defs>
block: i-bolt, i-temp, i-airplay, i-bt, i-wifi, i-vol, i-mute, i-volt, plus the
inline battery in the playing and charging screens). They are rebuilt here with
Pillow drawing calls instead of a path parser, because a parser would be a
second thing to be wrong and nobody would read its output either way -- these
calls are the drawing, and a reviewer can check them against the SVG by eye.

Four of the twelve are ours: lock, refresh, warning and the four-bar signal
meter have no symbol in the design, so they are drawn in its language -- a
24-unit grid, round caps, one stroke weight -- rather than borrowed from a
stock set that would not match.

Each icon is rasterised at the ONE size it is drawn on the panel. Scaling a
12 px icon at run time is how a 12 px icon becomes a smudge, and the flash cost
of a second size is a hundred bytes, so if a second size is ever wanted it is
added here as its own entry.

Run:  fontenv/bin/python tools/gen_icons.py [--sheet preview.png]
"""

import argparse
import math
import os
import sys

from PIL import Image, ImageDraw

# Supersample factor, and therefore also the placement grid: ImageDraw rounds
# every coordinate to a whole buffer pixel, so this is how finely a stroke can
# be positioned -- an eighth of a device pixel here, a quarter at 4x. That is
# what the extra samples buy. Sixteen samples would already be enough to fill a
# 4-bit coverage value; it is the three wifi arcs, whose spacing is 2.9 px and
# whose weight is 1.3, that want their edges placed more precisely than that.
SS = 8

# Stroke weight, in device pixels, for everything drawn as a line rather than a
# fill. The design uses 1.7-1.8 on a 24-unit grid, which is 0.85 px at the size
# these are actually drawn -- thin enough that antialiasing turns the whole
# stroke grey and the icon reads as a stain.
#
# 1.3 was picked by rendering the set at 1.15, 1.3, 1.5 and 1.7 and looking.
# At 1.15 a stroked icon barely reaches full coverage anywhere -- three pixels
# of the wifi fan, two of the volt ring -- so beside the filled bolt in the same
# row it reads a stop dimmer, as though it were disabled. 1.3 lets a stroke own
# a pixel when it lands on one, which closes that. 1.5 still reads; by 1.7 the
# speaker's two waves have closed into one band and the airplay screen's
# interior is a slot. 1.3 is the lighter of the two that work, and the closer to
# the design's own weight.
SW = 1.3


class Canvas:
    """A supersampled 8-bit coverage buffer. All coordinates are device pixels.

    Pillow's ImageDraw has no antialiasing at all, so supersampling is not an
    improvement here -- it is the only antialiasing there is. The BOX resize at
    the end is an exact area average, which is the definition of coverage.
    """

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new("L", (w * SS, h * SS), 0)
        self.d = ImageDraw.Draw(self.img)

    def _w(self, px):
        return max(1, int(round(px * SS)))

    def disc(self, cx, cy, r, v=255):
        self.d.ellipse([(cx - r) * SS, (cy - r) * SS,
                        (cx + r) * SS, (cy + r) * SS], fill=v)

    def poly(self, pts, v=255):
        self.d.polygon([(x * SS, y * SS) for x, y in pts], fill=v)

    def stroke(self, pts, w=SW, v=255, closed=False):
        """Polyline with round caps and round joins."""
        seq = list(pts) + ([pts[0]] if closed else [])
        self.d.line([(x * SS, y * SS) for x, y in seq], fill=v, width=self._w(w),
                    joint="curve")
        for x, y in seq:
            self.disc(x, y, w / 2.0, v)

    def rrect(self, x0, y0, x1, y1, r, v=255):
        self.d.rounded_rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS],
                                 radius=r * SS, fill=v)

    def rrect_outline(self, x0, y0, x1, y1, r, w=SW, v=255):
        """Outline drawn inside the box, the way Pillow does it and the way SVG
        does not. The box is the outer edge of the stroke."""
        self.d.rounded_rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS],
                                 radius=r * SS, outline=v, width=self._w(w))

    def arc(self, cx, cy, r, a0, a1, w=SW, v=255, caps=True):
        """Arc whose CENTRELINE sits at radius r. Pillow strokes inward from the
        bounding ellipse, so the box is grown by half a stroke to compensate."""
        ro = r + w / 2.0
        self.d.arc([(cx - ro) * SS, (cy - ro) * SS, (cx + ro) * SS, (cy + ro) * SS],
                   a0, a1, fill=v, width=self._w(w))
        if caps:
            for a in (a0, a1):
                t = math.radians(a)
                self.disc(cx + r * math.cos(t), cy + r * math.sin(t), w / 2.0, v)

    def mask(self):
        return self.img.resize((self.w, self.h), Image.Resampling.BOX)


# --------------------------------------------------------------------------
# The icons. Every coordinate below is in device pixels at the final size, so
# what you read is what lands on the panel; where a shape came from an SVG the
# comment gives the 24-unit design coordinates it was scaled from.
# --------------------------------------------------------------------------

SZ = 12          # the square icons: the design's .metric row at 11 px, rounded
                 # up to an even number so a 4bpp row packs into whole bytes
                 # with nothing wasted


def icon_airplay():
    """i-airplay: a screen with a triangle under it.

    The design's rect leaves a gap in its bottom edge for the triangle's apex to
    poke through (the path runs 5.5,17 ... 18.5,17). At 12 px that gap eats all
    but 1.7 px of the bottom edge and the box reads as broken, so here the box
    is closed and the triangle is moved clear of it. The silhouette is the same
    from arm's length and it survives the downsample.
    """
    c = Canvas(SZ, SZ)
    c.rrect_outline(0.55, 0.95, 11.45, 7.15, 1.9)
    c.poly([(6.0, 8.05), (9.05, 11.55), (2.95, 11.55)])
    return c


def icon_wifi():
    """i-wifi: three arcs over a dot, concentric on the dot.

    The design's arcs are 3.6 design units apart, which is 1.8 px here -- less
    than half a stroke of clear ground, so the fan closes into a wedge. The
    radii below are respaced to 2.9 px so the gaps survive; the fan is also
    narrowed to +/-36 degrees from twelve o'clock so the widest arc stays inside
    the box instead of being clipped by it.
    """
    c = Canvas(SZ, SZ)
    cx, cy = 6.0, 11.1
    for r in (8.9, 6.0, 3.1):
        c.arc(cx, cy, r, 234, 306)
    c.disc(cx, cy, 1.0)
    return c


def icon_bolt():
    """i-bolt, scaled 1:2 from the design's 24-unit path. Filled, not stroked,
    so it keeps its mass at this size."""
    c = Canvas(SZ, SZ)
    c.poly([(6.75, 1.0), (2.5, 6.9), (5.3, 6.9),
            (4.9, 11.0), (9.5, 5.1), (6.55, 5.1)])
    return c


def icon_volt():
    """i-volt: the bolt inside a ring.

    The design's inner bolt is 6 units wide in a 17.2-unit circle; held to that
    ratio at 12 px it comes back from the downsample as a dot -- which is what
    the first attempt produced. So the ring is pushed out to the edge of the box
    and held a shade under the common stroke weight, and the bolt is grown into
    the room that makes: 0.72 px per design unit leaves 0.7 px of clear ground
    between the bolt's tips and the ring, which is the least that survives.
    """
    c = Canvas(SZ, SZ)
    c.arc(6.0, 6.0, 4.95, 0, 360, w=1.2, caps=False)
    # design path 12.6,7 -> 9,12.6 -> 11.8,12.6 -> 11.2,17 -> 15,11.2 -> 12.1,11.2,
    # about its own centre (12,12)
    pts = [(12.6, 7.0), (9.0, 12.6), (11.8, 12.6), (11.2, 17.0), (15.0, 11.2), (12.1, 11.2)]
    c.poly([((x - 12.0) * 0.72 + 6.0, (y - 12.0) * 0.72 + 6.0) for x, y in pts])
    return c


def icon_thermometer():
    """i-temp: a hollow stem on a solid bulb.

    The design draws bulb and stem as one outline with the mercury as a second
    stroke inside it. That is four edges across a 5 px bulb here, so the bulb
    fills in solid and the icon becomes a pin. Keeping the stem hollow and the
    bulb solid keeps one edge where it does the work -- the silhouette still
    says thermometer, and the lumen is the only part that had to give.
    """
    c = Canvas(SZ, SZ)
    c.rrect_outline(4.25, 0.55, 7.75, 8.6, 1.7, w=1.15)
    c.disc(6.0, 8.95, 2.85)
    return c


def icon_battery():
    """The playing screen's inline battery, as an EMPTY shell.

    The design's SVG carries a fill rect sized to the charge, which a stored
    mask cannot do -- one bitmap is one state of charge. So this is the case and
    the terminal only, and the screen draws the charge into the hole with
    hk_gfx_rrect: 14 x 6 whole pixels at (2, 2) inside the icon, with a further
    half-covered pixel of wall on each side of that. A mask that lied about the
    charge would be worse than no icon.
    """
    c = Canvas(20, 10)
    c.rrect_outline(0.4, 0.5, 17.1, 9.5, 2.2, w=1.2)
    c.rrect(17.75, 3.25, 19.5, 6.75, 0.85)
    return c


def _speaker_cone(c, dx):
    """i-vol's cone, scaled 1:2 from the design and shifted to leave room for
    whatever goes to its right."""
    pts = [(4, 9.4), (7.4, 9.4), (12.6, 5.0), (12.6, 19.0), (7.4, 14.6), (4, 14.6)]
    c.poly([(x * 0.5 + dx, y * 0.5) for x, y in pts])


def icon_speaker():
    """i-vol: cone plus two waves.

    The design's waves are at radii 4.2 and 6.9 design units -- 1.35 px apart
    here, which is less than one stroke. They are respaced to 3.1 px and the
    whole group is pushed to the right edge of the box; that is the entire
    difference and it is the difference between two waves and one thick one.
    """
    c = Canvas(SZ, SZ)
    _speaker_cone(c, -1.4)
    for r in (2.9, 6.0):
        c.arc(4.6, 6.0, r, -44, 44)
    return c


def icon_speaker_mute():
    """i-mute: the same cone with a cross. The cross is 5.2 design units in the
    design, which is 2.6 px here -- too small to read as two strokes crossing,
    so it is opened out to 4.3 px in the space the waves were using."""
    c = Canvas(SZ, SZ)
    _speaker_cone(c, -1.6)
    c.stroke([(6.4, 3.9), (10.7, 8.2)], w=1.35)
    c.stroke([(10.7, 3.9), (6.4, 8.2)], w=1.35)
    return c


def icon_lock():
    """Ours. A solid body under a stroked shackle, in the design's stroke weight.

    The keyhole is a knockout rather than a drawn detail, because the body is
    6.5 px tall and a 1.6 px hole in it is the only way to spend those pixels
    and still be reading a lock.
    """
    c = Canvas(SZ, SZ)
    c.arc(6.0, 5.4, 2.7, 180, 360)
    c.rrect(1.5, 5.1, 10.5, 11.5, 1.6)
    c.disc(6.0, 7.7, 0.85, v=0)
    c.rrect(5.45, 7.6, 6.55, 9.9, 0.55, v=0)
    return c


def icon_bluetooth():
    """i-bt, the rune, scaled 1:2 from the design and then stretched 1.75x across
    and 1.25x down.

    At a straight 1:2 it is 4 px wide in a 12 px box and the two long diagonals
    sit inside one stroke width of each other, so the middle fills in. Widening
    it opens the two triangles that are the only thing telling this apart from a
    scribble; the stroke also stays under the common weight, because the two
    diagonals cross at the centre and that crossing is already the heaviest
    pixel in the set.
    """
    c = Canvas(SZ, SZ)
    pts = [(8, 7.5), (16, 16.5), (12, 20), (12, 4), (16, 7.5), (8, 16.5)]
    c.stroke([((x * 0.5 - 6.0) * 1.75 + 6.0, (y * 0.5 - 6.0) * 1.25 + 6.0)
              for x, y in pts], w=1.15)
    return c


def icon_refresh():
    """Ours. An open ring with an arrowhead at the loose end.

    The gap is 120 degrees rather than the 30 a big refresh glyph gets away
    with: at a 3.4 px radius a narrow gap closes up in the downsample and the
    arrow ends up chasing its own tail.
    """
    c = Canvas(SZ, SZ)
    cx, cy, r = 6.0, 6.3, 3.4
    c.arc(cx, cy, r, 110, 350, caps=False)
    a = math.radians(350.0)
    px, py = cx + r * math.cos(a), cy + r * math.sin(a)
    tx, ty = -math.sin(a), math.cos(a)      # clockwise tangent
    nx, ny = -ty, tx
    c.poly([(px + tx * 2.1, py + ty * 2.1),
            (px - tx * 1.1 + nx * 1.85, py - ty * 1.1 + ny * 1.85),
            (px - tx * 1.1 - nx * 1.85, py - ty * 1.1 - ny * 1.85)])
    return c


def icon_warning():
    """Ours. A solid triangle with the mark knocked out of it.

    Solid rather than the outlined triangle the rest of the set would suggest:
    an outline at this size leaves 4 px of interior for a bar and a dot, and the
    bar then touches the outline. Filling the triangle and cutting the mark out
    of it puts every pixel of contrast on the one thing that has to be read.
    """
    c = Canvas(SZ, SZ)
    # inset vertices, then re-inflated by the round join, so the corners come
    # back rounded at the outer triangle rather than as antialiased needles
    c.stroke([(6.0, 2.3), (9.95, 9.55), (2.05, 9.55)], w=1.4, closed=True)
    c.poly([(6.0, 2.3), (9.95, 9.55), (2.05, 9.55)])
    c.rrect(5.35, 4.3, 6.65, 7.2, 0.65, v=0)
    c.disc(6.0, 8.6, 0.78, v=0)
    return c


# Order must match hk_icon_id_t in hk_icons.h. A mismatch here is a wrong icon
# on the panel and nothing else complains, so the generated table is emitted
# with the enum names beside it for the next reader to check against.
ICONS = [
    ("HK_ICON_AIRPLAY",      "airplay",      icon_airplay),
    ("HK_ICON_WIFI",         "wifi",         icon_wifi),
    ("HK_ICON_BOLT",         "bolt",         icon_bolt),
    ("HK_ICON_VOLT",         "volt",         icon_volt),
    ("HK_ICON_THERMOMETER",  "thermometer",  icon_thermometer),
    ("HK_ICON_BATTERY",      "battery",      icon_battery),
    ("HK_ICON_SPEAKER",      "speaker",      icon_speaker),
    ("HK_ICON_SPEAKER_MUTE", "speaker-mute", icon_speaker_mute),
    ("HK_ICON_LOCK",         "lock",         icon_lock),
    ("HK_ICON_BLUETOOTH",    "bluetooth",    icon_bluetooth),
    ("HK_ICON_REFRESH",      "refresh",      icon_refresh),
    ("HK_ICON_WARNING",      "warning",      icon_warning),
]


def pack4(img):
    """8-bit coverage to the 4bpp form hk_gfx_blit_a4 reads: two pixels per
    byte, high nibble first, rows padded to whole bytes."""
    w, h = img.size
    px = img.load()
    stride = (w + 1) // 2
    out = bytearray()
    for y in range(h):
        row = bytearray(stride)
        for x in range(w):
            v = int(round(px[x, y] * 15.0 / 255.0))
            if x % 2 == 0:
                row[x // 2] |= (v & 0xF) << 4
            else:
                row[x // 2] |= v & 0xF
        out += row
    return bytes(out), stride


def emit(path, built):
    lines = []
    a = lines.append
    a("/*")
    a(" * Generated by tools/gen_icons.py -- do not edit.")
    a(" *")
    a(" * Shapes come from the design's SVG symbols in hk-screen.html; the")
    a(" * generator carries the note on what each one had to give up to survive")
    a(" * being 12 pixels tall. Regenerate rather than patching nibbles here:")
    a(" * a hand edit is invisible the next time anyone runs the tool.")
    a(" */")
    a('#include "hk_icons.h"')
    a("")
    a("extern const hk_icon_t hk_icons_generated[HK_ICON_COUNT];")
    a("")
    total = 0
    for enum_name, name, img, data, stride in built:
        w, h = img.size
        total += len(data)
        a("/* %s: %dx%d, %d bytes */" % (enum_name, w, h, len(data)))
        a("static const uint8_t cov_%s[%d] = {" % (name.replace("-", "_"), len(data)))
        for y in range(h):
            row = data[y * stride:(y + 1) * stride]
            a("    " + " ".join("0x%02x," % b for b in row))
        a("};")
        a("")
    a("const hk_icon_t hk_icons_generated[HK_ICON_COUNT] = {")
    for enum_name, name, img, data, stride in built:
        w, h = img.size
        a('    [%s] = { "%s", %d, %d, cov_%s },'
          % (enum_name, name, w, h, name.replace("-", "_")))
    a("};")
    a("")
    a("/* %d bytes of coverage in total. */" % total)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return total


def sheet(path, built):
    """A contact sheet at 1:1 and at 6:1, for looking at before believing."""
    pad, zoom = 8, 6
    cw = max(img.size[0] for _, _, img, _, _ in built) * zoom + pad * 2
    ch = max(img.size[1] for _, _, img, _, _ in built) * zoom + pad * 2 + 14
    cols = 6
    rows = (len(built) + cols - 1) // cols
    out = Image.new("RGB", (cols * cw, rows * ch), (10, 9, 18))
    for i, (_, name, img, _, _) in enumerate(built):
        big = img.resize((img.size[0] * zoom, img.size[1] * zoom),
                         Image.Resampling.NEAREST)
        tint = Image.new("RGB", big.size, (0xec, 0xea, 0xf3))
        cell = Image.new("RGB", big.size, (10, 9, 18))
        cell.paste(tint, (0, 0), big)
        cx = (i % cols) * cw + pad
        cy = (i // cols) * ch + pad
        out.paste(cell, (cx, cy))
        d = ImageDraw.Draw(out)
        d.text((cx, cy + big.size[1] + 3), name, fill=(0xa5, 0xa1, 0xb4))
        one = Image.new("RGB", img.size, (10, 9, 18))
        one.paste(Image.new("RGB", img.size, (0xec, 0xea, 0xf3)), (0, 0), img)
        out.paste(one, (cx + big.size[0] - img.size[0], cy + big.size[1] + 2))
    out.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=None)
    ap.add_argument("--sheet", default=None)
    args = ap.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    out = args.out or os.path.join(here, "..", "hk_icons_data.c")

    built = []
    for enum_name, name, fn in ICONS:
        img = fn().mask()
        data, stride = pack4(img)
        built.append((enum_name, name, img, data, stride))

    total = emit(out, built)
    print("wrote %s (%d icons, %d bytes of coverage)"
          % (os.path.normpath(out), len(built), total))
    for enum_name, name, img, data, _ in built:
        print("  %-14s %2dx%-2d %4d bytes" % (name, img.size[0], img.size[1], len(data)))
    if args.sheet:
        sheet(args.sheet, built)
        print("sheet: %s" % args.sheet)


if __name__ == "__main__":
    sys.exit(main())
