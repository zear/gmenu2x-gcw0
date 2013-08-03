/***************************************************************************
 *   Copyright (C) 2006 by Massimiliano Torromeo                           *
 *   massimiliano.torromeo@gmail.com                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef SURFACE_H
#define SURFACE_H

#include "font.h"

#include <SDL.h>
#include <string>

struct RGBAColor {
	unsigned short r,g,b,a;
};

RGBAColor strtorgba(const std::string &strColor);

/**
	Wrapper around SDL_Surface
	@author Massimiliano Torromeo <massimiliano.torromeo@gmail.com>
*/
class Surface {
public:
	static Surface *openOutputSurface(int width, int height, int bitsperpixel);
	static Surface *emptySurface(int width, int height);
	static Surface *loadImage(const std::string &img,
			const std::string &skin="");

	Surface(Surface *s);
	~Surface();

	/** Converts the underlying surface to the same pixel format as the frame
	  * buffer, for faster blitting. This removes the alpha channel if the
	  * image has done.
	  */
	void convertToDisplayFormat();

	int width() { return raw->w; }
	int height() { return raw->h; }

	void flip();

	void clearClipRect();
	void setClipRect(int x, int y, int w, int h);
	void setClipRect(SDL_Rect rect);

	bool blit(Surface *destination, int x, int y, int w=0, int h=0, int a=-1);
	bool blit(Surface *destination, SDL_Rect container, Font::HAlign halign = Font::HAlignLeft, Font::VAlign valign = Font::VAlignTop);
	bool blitCenter(Surface *destination, int x, int y, int w=0, int h=0, int a=-1);
	bool blitRight(Surface *destination, int x, int y, int w=0, int h=0, int a=-1);

	void write(Font *font, const std::string &text, int x, int y,
			Font::HAlign halign = Font::HAlignLeft,
			Font::VAlign valign = Font::VAlignTop) {
		font->write(this, text, x, y, halign, valign);
	}

	int box(Sint16 x, Sint16 y, Uint16 w, Uint16 h, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
	int box(Sint16 x, Sint16 y, Uint16 w, Uint16 h, Uint8 r, Uint8 g, Uint8 b);
	int box(Sint16 x, Sint16 y, Uint16 w, Uint16 h, RGBAColor);
	int box(SDL_Rect, RGBAColor);
	int rectangle(Sint16, Sint16, Uint16, Uint16, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
	int rectangle(Sint16, Sint16, Uint16, Uint16, RGBAColor);
	int rectangle(SDL_Rect, RGBAColor);
	int hline(Sint16 x, Sint16 y, Uint16 h, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

private:
	Surface(SDL_Surface *raw, bool freeWhenDone);
	bool blit(SDL_Surface *destination, int x, int y, int w=0, int h=0, int a=-1);
	bool blitCenter(SDL_Surface *destination, int x, int y, int w=0, int h=0, int a=-1);
	bool blitRight(SDL_Surface *destination, int x, int y, int w=0, int h=0, int a=-1);

	SDL_Surface *raw;
	bool freeWhenDone;
	int halfW, halfH;

	// For direct access to "raw".
	friend class Font;
};

#endif
