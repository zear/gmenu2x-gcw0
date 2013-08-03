// Original font class was replaced by an SDL_ttf based one by Paul Cercueil.
// License: GPL version 2 or later.

#ifndef FONT_H
#define FONT_H

#include <SDL_ttf.h>
#include <string>

class Surface;

class Font {
public:
	enum HAlign { HAlignLeft, HAlignRight,  HAlignCenter };
	enum VAlign { VAlignTop,  VAlignBottom, VAlignMiddle };

	Font(const std::string &font);
	~Font();

	int getTextWidth(const char *text);

	int getTextWidth(const std::string& text)
	{
		return getTextWidth(text.c_str());
	}

	int getHeight()
	{
		return fontheight;
	}

	void write(Surface *surface,
				const std::string &text, int x, int y,
				HAlign halign = HAlignLeft, VAlign valign = VAlignTop);

private:
	void writeLine(Surface *surface, const char *text,
				int x, int y, HAlign halign, VAlign valign);

	TTF_Font *font;
	unsigned int fontheight;
};

#endif /* FONT_H */
