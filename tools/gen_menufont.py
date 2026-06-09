#!/usr/bin/env python3
"""Generate engine/wii/menufont.h - minimal GBK bitmap font for Wii menu."""

import os
import struct
from PIL import Image, ImageDraw, ImageFont

# All unique Chinese characters used in Wii menu strings
CHARS = "开始游戏音乐播放查看日志退出无模组文件夹中未找到"

WIDTH = 12
HEIGHT = 12
OUT = os.path.join(os.path.dirname(__file__), "..", "engine", "wii", "menufont.h")

# Try common Windows Chinese fonts
FONT_CANDIDATES = [
    r"C:\Windows\Fonts\simhei.ttf",
    r"C:\Windows\Fonts\msyh.ttc",
    r"C:\Windows\Fonts\simsun.ttc",
    r"C:\Windows\Fonts\simkai.ttf",
]


def find_font():
    for path in FONT_CANDIDATES:
        if os.path.isfile(path):
            return path
    raise SystemExit("No Chinese TTF font found on system")


def char_to_gbk(ch):
    return ch.encode("gbk")


def glyph_to_bytes(img):
    """Convert 12x12 image to row-major bitmask bytes (12 bytes per row, 12 rows)."""
    rows = []
    for y in range(HEIGHT):
        row = 0
        for x in range(WIDTH):
            if img.getpixel((x, y)) > 128:
                row |= 1 << (WIDTH - 1 - x)
        rows.append(row)
    return rows


def render_glyph(font, ch):
    img = Image.new("L", (WIDTH, HEIGHT), 0)
    draw = ImageDraw.Draw(img)
    # Center glyph in cell
    bbox = draw.textbbox((0, 0), ch, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    ox = (WIDTH - tw) // 2 - bbox[0]
    oy = (HEIGHT - th) // 2 - bbox[1]
    draw.text((ox, oy), ch, fill=255, font=font)
    return img


def main():
    font_path = find_font()
    font = ImageFont.truetype(font_path, 11)

    entries = []
    for ch in CHARS:
        gbk = char_to_gbk(ch)
        if len(gbk) != 2:
            continue
        code = (gbk[0] << 8) | gbk[1]
        rows = glyph_to_bytes(render_glyph(font, ch))
        entries.append((code, ch, rows))

    entries.sort(key=lambda e: e[0])

    lines = [
        "/*",
        " * OpenBOR Wii menu font - auto-generated, do not edit by hand.",
        " * GBK 12x12 bitmap glyphs for Chinese menu strings.",
        " */",
        "#ifndef MENUFONT_H",
        "#define MENUFONT_H",
        "",
        "#define MENUFONT_WIDTH  12",
        "#define MENUFONT_HEIGHT 12",
        "",
        "typedef struct {",
        "\tunsigned short code;",
        "\tunsigned short data[12];",
        "} MenuFontGlyph;",
        "",
        "static const MenuFontGlyph menu_font_glyphs[] = {",
    ]

    for code, ch, rows in entries:
        hex_rows = ", ".join(f"0x{r:03X}" for r in rows)
        lines.append(f"\t{{ 0x{code:04X}, {{ {hex_rows} }} }}, /* {ch} */")

    lines += [
        "};",
        "",
        "#define MENUFONT_GLYPH_COUNT (sizeof(menu_font_glyphs) / sizeof(menu_font_glyphs[0]))",
        "",
        "static inline const unsigned short *menu_font_lookup(unsigned char lead, unsigned char trail)",
        "{",
        "\tunsigned short code = ((unsigned short)lead << 8) | trail;",
        "\tunsigned i;",
        "\tfor(i = 0; i < MENUFONT_GLYPH_COUNT; i++)",
        "\t{",
        "\t\tif(menu_font_glyphs[i].code == code)",
        "\t\t\treturn menu_font_glyphs[i].data;",
        "\t}",
        "\treturn 0;",
        "}",
        "",
        "#endif",
        "",
    ]

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    print(f"Wrote {len(entries)} glyphs to {OUT}")


if __name__ == "__main__":
    main()
