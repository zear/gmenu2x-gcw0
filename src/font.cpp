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
	if (!TTF_WasInit() && TTF_Init() < 0) {
		ERROR("Unable to init SDL_ttf library\n");
		return nullptr;
	}

	TTF_Font *font = TTF_OpenFont(TTF_FONT, TTF_FONT_SIZE);
	if (!font) {
		ERROR("Unable to open font\n");
		return nullptr;
	}

	return new Font(font);
}

Font::Font(TTF_Font *font)
	: font(font)
{
	fontheight = TTF_FontHeight(font);
}

Font::~Font()
{
	TTF_CloseFont(font);
	TTF_Quit();
}

int Font::getTextWidth(const char *text)
{
	int w, h;
	TTF_SizeUTF8(font, text, &w, &h);
	return w;
}

void Font::write(Surface *surface, const string &text,
			int x, int y, HAlign halign, VAlign valign)
{
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
	SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, color);

	SDL_Rect rect = { (Sint16) x, (Sint16) (y - 1), 0, 0 };
	SDL_BlitSurface(s, NULL, surface->raw, &rect);

	/* Note: rect.x / rect.y are reset everytime because SDL_BlitSurface
	 * will modify them if negative */
	rect.x = x;
	rect.y = y + 1;
	SDL_BlitSurface(s, NULL, surface->raw, &rect);

	rect.x = x - 1;
	rect.y = y;
	SDL_BlitSurface(s, NULL, surface->raw, &rect);

	rect.x = x + 1;
	rect.y = y;
	SDL_BlitSurface(s, NULL, surface->raw, &rect);
	SDL_FreeSurface(s);

	rect.x = x;
	rect.y = y;
	color.r = 0xff;
	color.g = 0xff;
	color.b = 0xff;

	s = TTF_RenderUTF8_Blended(font, text, color);
	SDL_BlitSurface(s, NULL, surface->raw, &rect);
	SDL_FreeSurface(s);
}
