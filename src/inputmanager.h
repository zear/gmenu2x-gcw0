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

#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <SDL.h>
#include <string>
#include <vector>

class Menu;

enum EventCode {
	REMOVE_LINKS,
	OPEN_PACKAGE,
	OPEN_PACKAGES_FROM_DIR,
	REPAINT_MENU,
};

class InputManager {
public:
	enum Button {
		UP, DOWN, LEFT, RIGHT,
		ACCEPT, CANCEL,
		ALTLEFT, ALTRIGHT,
		MENU, SETTINGS,
		REPAINT,
	};
	#define BUTTON_TYPE_SIZE 10

	enum ButtonState { PRESSED, RELEASED };
	struct ButtonEvent {
		Button button;
		ButtonState state;
	};

	InputManager();
	~InputManager();

	void init(const std::string &conffile, Menu *menu);
	bool waitForEvent(ButtonEvent *event);
	Button waitForPressedButton();
	Button waitForReleasedButton();
	bool pollEvent(ButtonEvent *event);

private:
	enum ButtonSource { UNMAPPED, KEYBOARD, JOYSTICK };
	struct ButtonMapEntry {
		ButtonSource source;
		unsigned int code;
	};

	Menu *menu;

	void readConfFile(const std::string &conffile);
	bool getEvent(ButtonEvent *bevent, bool wait);
	Button waitForButton(ButtonState state);

	ButtonMapEntry buttonMap[BUTTON_TYPE_SIZE];
#ifndef SDL_JOYSTICK_DISABLED
	std::vector<SDL_Joystick *> joysticks;
#endif
};

#endif
