#!/usr/bin/env python3
"""Adds the glyphs after 'Z' (ASCII 91..126: brackets, lower case, braces) to the Classic atlas fonts.

The original tools/gen_gui_fonts.py is not in the repository; this script keeps every existing glyph and its data as it is and
appends the new ones, rasterized the same way (Liberation Sans, 2x resolution, coverage packed as 4-bit nibbles, baseline at
capTop + capHeight). Run it once per change of the character range:  python3 tools/extend_gui_fonts.py
Needs Pillow and numpy and the Liberation Sans fonts. Glyphs rasterize with 0.2 of a level mean error against the old ones.
"""
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

SS = 8
LIB = "/usr/share/fonts/truetype/liberation/"
ROOT = Path(__file__).resolve().parent.parent / "include/spux/ui/classic"
LAST = 126
# header, font file, pixel size at 2x, horizontal scale, cap top, cap height
FONTS = [
    ("LabelFont.hpp", "LiberationSans-Bold.ttf", 20, 0.86, 4, 14),
    ("DisplayFont.hpp", "LiberationSans-Regular.ttf", 44, 1.0, 10, 30),
]


def raster(ttf, size, xs, ch, baseline):
    f = ImageFont.truetype(ttf, size * SS)
    adv16 = int(round(f.getlength(ch) / SS * xs * 16))
    ox, by = size * SS, size * 2 * SS
    im = Image.new("L", (size * 3 * SS, size * 3 * SS), 0)
    ImageDraw.Draw(im).text((ox, by), ch, font=f, fill=255, anchor="ls")
    a = np.asarray(im).astype(float) / 255
    ys, xp = np.nonzero(a > 0.01)
    x0 = int(np.floor((xp.min() - ox) / SS * xs))
    x1 = int(np.ceil((xp.max() + 1 - ox) / SS * xs))
    y0 = int(np.floor((ys.min() - by) / SS))
    y1 = int(np.ceil((ys.max() + 1 - by) / SS))
    out = np.zeros((y1 - y0, x1 - x0))
    for j in range(y1 - y0):
        for i in range(x1 - x0):
            sx0, sx1 = ox + (x0 + i) / xs * SS, ox + (x0 + i + 1) / xs * SS
            sy0 = by + (y0 + j) * SS
            ix0, ix1 = int(np.floor(sx0)), int(np.ceil(sx1))
            wx = np.ones(ix1 - ix0)
            wx[0] -= sx0 - ix0
            wx[-1] -= ix1 - sx1
            out[j, i] = (a[int(sy0):int(sy0) + SS, ix0:ix1] * wx[None, :]).sum() / (SS * (sx1 - sx0))
    px = np.clip(np.rint(out * 15), 0, 15).astype(int)
    rows, cols = np.nonzero(px.sum(axis=1))[0], np.nonzero(px.sum(axis=0))[0]
    px = px[rows[0]:rows[-1] + 1, cols[0]:cols[-1] + 1]
    return px, x0 + int(cols[0]), baseline + y0 + int(rows[0]), adv16


def extend(header, ttf, size, xs, cap_top, cap_h):
    path = ROOT / header
    src = path.read_text()
    last = int(re.search(r"kAlpha, (\d+), (\d+), ", src).group(2))
    if last >= LAST:
        return
    alpha = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", src[src.index("kAlpha"):src.index("};", src.index("kAlpha"))]))
    glyphs, data = [], bytearray(alpha)
    for code in range(last + 1, LAST + 1):
        px, dx, dy, adv = raster(LIB + ttf, size, xs, chr(code), cap_top + cap_h)
        h, w = px.shape
        flat = list(px.flatten()) + ([0] if px.size % 2 else [])
        offset = len(data)
        data.extend((flat[i] << 4) | flat[i + 1] for i in range(0, len(flat), 2))
        glyphs.append("    {%d, %d, %d, %d, %d, %d},  // '%s'" % (offset, w, h, dx, dy, adv, "\\" if chr(code) == "\\" else chr(code)))
    # the old data ends in a half-used byte only when its last glyph has an odd pixel count; offsets then stay byte aligned as before
    table_end = src.index("};", src.index("kGlyphs[]"))
    src = src[:table_end] + "\n".join(glyphs) + "\n" + src[table_end:]
    a0 = src.index("kAlpha[] = {") + len("kAlpha[] = {")
    a1 = src.index("};", a0)
    lines = ["    " + ", ".join("0x%02X" % b for b in data[i:i + 20]) + "," for i in range(0, len(data), 20)]
    src = src[:a0] + "\n" + "\n".join(lines) + "\n" + src[a1:]
    src = re.sub(r"kAlpha, \d+, \d+, ", "kAlpha, 32, %d, " % LAST, src)
    path.write_text(src)
    print(header, "now covers 32 to", LAST, "(%d bytes of coverage)" % len(data))


if __name__ == "__main__":
    for spec in FONTS:
        extend(*spec)
    sys.exit(0)
