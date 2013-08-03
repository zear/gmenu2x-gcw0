// Based on SFont by Karl Bartel.
// Adapted to C++ by Massimiliano Torromeo.
// Refactored by Maarten ter Huurne and several others (see git log).
// License: GPL version 2 or later.

#ifndef ASFONT_H
#define ASFONT_H

#include <SDL_ttf.h>
#include <string>
#include <vector>

class Surface;

class ASFont {
public:
	enum HAlign { HAlignLeft, HAlignRight,  HAlignCenter };
	enum VAlign { VAlignTop,  VAlignBottom, VAlignMiddle };

	ASFont(const std::string &font);
	~ASFont();

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

#endif /* ASFONT_H */
