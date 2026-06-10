/*
 * OpenBOR Wii menu font - auto-generated, do not edit by hand.
 * GBK 12x12 bitmap glyphs (GB2312 + menu strings).
 */
#ifndef MENUFONT_H
#define MENUFONT_H

#define MENUFONT_WIDTH  12
#define MENUFONT_HEIGHT 12

typedef struct {
	unsigned short code;
	unsigned short data[12];
} MenuFontGlyph;

extern const MenuFontGlyph menu_font_glyphs[];
#define MENUFONT_GLYPH_COUNT 7478

static inline const unsigned short *menu_font_lookup(unsigned char lead, unsigned char trail)
{
	unsigned short code = ((unsigned short)lead << 8) | trail;
	int lo = 0;
	int hi = MENUFONT_GLYPH_COUNT - 1;

	while(lo <= hi)
	{
		int mid = (lo + hi) >> 1;
		if(menu_font_glyphs[mid].code < code)
		{
			lo = mid + 1;
		}
		else if(menu_font_glyphs[mid].code > code)
		{
			hi = mid - 1;
		}
		else
		{
			return menu_font_glyphs[mid].data;
		}
	}
	return 0;
}

#endif
