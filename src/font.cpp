#include "font.h"

#include "debug.h"
#include "surface.h"
#include "utilities.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>

/* TODO: Let the theme choose the font and font size */
#define TTF_FONT "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf"
#define TTF_FONT_SIZE 12

using namespace std;

Font *Font::defaultFont()
{
	return new Font(TTF_FONT, TTF_FONT_SIZE);
}

Font::Font(const std::string &path, unsigned int size)
{
	mainFont = outlineFont = nullptr;
	fontheight = 1;

	/* Note: TTF_Init and TTF_Quit perform reference counting, so call them
	 * both unconditionally for each font. */
	if (TTF_Init() < 0) {
		ERROR("Unable to init SDL_ttf library\n");
		return;
	}

	mainFont = TTF_OpenFont(path.c_str(), size);
	if (!mainFont) {
		ERROR("Unable to open font\n");
		TTF_Quit();
		return;
	}

	outlineFont = TTF_OpenFont(path.c_str(), size);
	if (!outlineFont) {
		ERROR("Unable to open font\n");
		TTF_CloseFont(mainFont);
		mainFont = nullptr;
		TTF_Quit();
		return;
	}

	fontheight = TTF_FontHeight(mainFont);
	TTF_SetFontOutline(outlineFont, 1);
}

Font::~Font()
{
	if (mainFont) {
		TTF_CloseFont(mainFont);
		TTF_CloseFont(outlineFont);
		TTF_Quit();
	}
}

int Font::getTextWidth(const char *text)
{
	if (mainFont) {
		int w, h;
		TTF_SizeUTF8(mainFont, text, &w, &h);
		return w;
	}
	else return 1;
}

void Font::write(Surface *surface, const string &text,
			int x, int y, HAlign halign, VAlign valign)
{
	if (!mainFont) {
		return;
	}

	if (text.find("\n", 0) == string::npos) {
		writeLine(surface, text.c_str(), x, y, halign, valign);
		return;
	}

	vector<string> v;
	split(v, text, "\n");

	for (vector<string>::const_iterator it = v.begin(); it != v.end(); it++) {
		writeLine(surface, it->c_str(), x, y, halign, valign);
		y += fontheight;
	}
}

void Font::writeLine(Surface *surface, const char *text,
				int x, int y, HAlign halign, VAlign valign)
{
	if (!mainFont) {
		return;
	}

	switch (halign) {
	case HAlignLeft:
		break;
	case HAlignCenter:
		x -= getTextWidth(text) / 2;
		break;
	case HAlignRight:
		x -= getTextWidth(text);
		break;
	}

	switch (valign) {
	case VAlignTop:
		break;
	case VAlignMiddle:
		y -= fontheight / 2;
		break;
	case VAlignBottom:
		y -= fontheight;
		break;
	}

	SDL_Color color = { 0, 0, 0, 0 };
	SDL_Surface *s = TTF_RenderUTF8_Blended(outlineFont, text, color);

	SDL_Rect rect = { (Sint16) (x - 1), (Sint16) (y - 1), 0, 0 };
	SDL_BlitSurface(s, NULL, surface->raw, &rect);
	SDL_FreeSurface(s);

	color.r = color.g = color.b = 255;
	/* Note: rect.x / rect.y are reset everytime because SDL_BlitSurface
	 * will modify them if negative */
	rect.x = x;
	rect.y = y;
	s = TTF_RenderUTF8_Blended(mainFont, text, color);
	SDL_BlitSurface(s, NULL, surface->raw, &rect);
	SDL_FreeSurface(s);
}
