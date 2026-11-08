#!/usr/bin/env python3
"""Make the clock face's big digits as an LVGL font (lv_font_fmt_txt, 4 bpp, plain bitmaps).

LVGL's built-in Montserrat stops at 48 px; the clock face wants its digits much bigger, and scaling
the 48 px font up blurs it. This draws the glyphs "-./0123456789:" from the same TTF LVGL makes its
built-in fonts from, at the size asked, and writes them in the format LVGL 9 reads:
  - glyph bitmaps: 4 bits per pixel, rows run on without padding, the first pixel in the high nibble
    (as lv_font_get_bitmap_fmt_txt reads them);
  - advances in 1/16 px, offsets from the baseline (up is positive), as lv_font_conv writes them;
  - a line height just as tall as these glyphs (no room for letters that aren't there).

    python3 tools/make_clock_font.py \\
        .pio/libdeps/advance-hmi/lvgl/scripts/built_in_font/Montserrat-Medium.ttf 128 src/ui/font_clock_128.c

Needs Pillow (python3-pillow).
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FIRST, LAST = 0x2D, 0x3A  # "-./0123456789:": one range, so a plain cmap covers it


def main(ttf, size, out):
    size = int(size)
    name = Path(out).stem
    font = ImageFont.truetype(ttf, size)
    ascent, _ = font.getmetrics()
    bitmap, glyphs = [], []
    for cp in range(FIRST, LAST + 1):
        ch = chr(cp)
        x0, y0, x1, y1 = font.getbbox(ch)  # from the left and the ascender line (anchor "la")
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        start = len(bitmap)
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255)
            nibbles = [(v * 15 + 127) // 255 for v in img.tobytes()]  # 0..255 -> 0..15, row by row
            if len(nibbles) % 2:
                nibbles.append(0)
            bitmap += [nibbles[i] << 4 | nibbles[i + 1] for i in range(0, len(nibbles), 2)]
        glyphs.append({"ch": ch, "index": start, "adv": round(font.getlength(ch) * 16),
                       "w": w, "h": h, "ox": x0, "oy": ascent - y1})

    # The line holds what a clock shows: "." and "/" are only there to keep one range (the "/" hangs
    # below the baseline and would make the line taller for nothing).
    drawn = [g for g in glyphs if g["h"] and g["ch"] in "-0123456789:"]
    base_line = max(0, -min(g["oy"] for g in drawn))
    line_height = max(g["oy"] + g["h"] for g in drawn) + base_line
    for g in glyphs:
        assert g["adv"] < 4096 and g["w"] < 256 and g["h"] < 256 and -128 <= g["ox"] < 128 and -128 <= g["oy"] < 128, g

    rows = []
    for g in glyphs:
        rows.append(f'    /* U+{ord(g["ch"]):04X} "{g["ch"]}" */')
        data = bitmap[g["index"]:g["index"] + (g["w"] * g["h"] + 1) // 2]
        for i in range(0, len(data), 16):
            rows.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
    dsc = ["    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,"]
    for g in glyphs:
        dsc.append(f'    {{.bitmap_index = {g["index"]}, .adv_w = {g["adv"]}, .box_w = {g["w"]}, .box_h = {g["h"]}, '
                   f'.ofs_x = {g["ox"]}, .ofs_y = {g["oy"]}}} /* "{g["ch"]}" */,')

    Path(out).write_text(f'''/* {name}: Montserrat Medium {size} px, 4 bpp, only "-./0123456789:", for the clock face.
 * Made by tools/make_clock_font.py from LVGL's own Montserrat-Medium.ttf (scripts/built_in_font):
 * make it again with that script rather than editing this file. */
#include "lvgl.h"

static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {{
{chr(10).join(rows)}
}};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
{chr(10).join(dsc)}
}};

static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = 0x{FIRST:X}, .range_length = {LAST - FIRST + 1}, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }}
}};

static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
}};

const lv_font_t {name} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {line_height}, /* just these glyphs: no letters, so no room for them */
    .base_line = {base_line},
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = {-size // 12},
    .underline_thickness = {max(1, size // 24)},
    .dsc = &font_dsc,
}};
''')
    print(f"{out}: {len(glyphs)} glyphs, {len(bitmap)} bytes of bitmaps, line height {line_height}, base line {base_line}")
    for g in glyphs:
        if g["ch"] in "08:":
            print(f'  "{g["ch"]}": advance {g["adv"] / 16:.1f} px, box {g["w"]} x {g["h"]}, offset {g["ox"]}, {g["oy"]}')


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
