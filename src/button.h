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

#include "FastDelegate.h"

#include <SDL.h>

typedef fastdelegate::FastDelegate0<> ButtonAction;
class Touchscreen;

class Button {
protected:
	Touchscreen &ts;
	ButtonAction action;
	SDL_Rect rect;
	bool doubleClick;
	int lastTick;

public:
	Button(Touchscreen &ts, bool doubleClick = false);
	virtual ~Button() {};

	SDL_Rect getRect();
	void setSize(int w, int h);
	virtual void setPosition(int x, int y);

	virtual void paint();
	virtual bool paintHover();

	bool isPressed();
	bool isReleased();
	bool handleTS();

	void exec();
	void voidAction() {};
	void setAction(ButtonAction action);
};

#endif // BUTTON_H
