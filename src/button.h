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

#ifndef BUTTON_H
#define BUTTON_H

#include "delegate.h"

#include <SDL.h>

class Touchscreen;

class Button {
public:
	SDL_Rect getRect();
	virtual void setPosition(int x, int y);

	bool isPressed();
	bool handleTS();

protected:
	Button(Touchscreen &ts, bool doubleClick = false);
	virtual ~Button() {};

	void setSize(int w, int h);

	function_t action;
	SDL_Rect rect;

private:
	bool isReleased();
	void voidAction() {};

	Touchscreen &ts;
	bool doubleClick;
	int lastTick;
};

#endif // BUTTON_H
