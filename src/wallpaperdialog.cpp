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

#include "wallpaperdialog.h"

#include "debug.h"
#include "buttonbox.h"
#include "filelister.h"
#include "gmenu2x.h"
#include "iconbutton.h"
#include "surface.h"
#include "utilities.h"

#include <iostream>

using namespace std;

WallpaperDialog::WallpaperDialog(GMenu2X *gmenu2x, Touchscreen &ts_)
	: Dialog(gmenu2x)
	, ts(ts_)
{
}

bool WallpaperDialog::exec()
{
	bool close = false, result = true;

	FileLister fl;
	fl.setFilter("png");

	string filepath = GMenu2X::getHome() + "/skins/"
	  	+ gmenu2x->confStr["skin"] + "/wallpapers";
	if (fileExists(filepath))
	  fl.setPath(filepath, true);

	filepath = GMENU2X_SYSTEM_DIR "/skins/"
	  	+ gmenu2x->confStr["skin"] + "/wallpapers";
	if (fileExists(filepath)) {
		fl.setPath(filepath, false);
		fl.browse(false);
	}

	if (gmenu2x->confStr["skin"] != "Default") {
		filepath = GMenu2X::getHome() + "/skins/Default/wallpapers";
		if (fileExists(filepath)) {
			fl.setPath(filepath, false);
			fl.browse(false);
		}

		filepath = GMENU2X_SYSTEM_DIR "/skins/Default/wallpapers";
		if (fileExists(filepath)) {
			fl.setPath(filepath, false);
			fl.browse(false);
		}
	}

	vector<string> wallpapers = fl.getFiles();

	DEBUG("Wallpapers: %i\n", wallpapers.size());

	uint i, selected = 0, firstElement = 0, iY;

	ButtonBox buttonbox(gmenu2x);
	buttonbox.add(new IconButton(gmenu2x, ts, "skin:imgs/buttons/accept.png", gmenu2x->tr["Select wallpaper"]));
	buttonbox.add(new IconButton(gmenu2x, ts, "skin:imgs/buttons/cancel.png", gmenu2x->tr["Exit"]));

	unsigned int top, height;
	tie(top, height) = gmenu2x->getContentArea();

	int fontheight = gmenu2x->font->getHeight();
	unsigned int nb_elements = height / fontheight;

	while (!close) {
		if (selected > firstElement + nb_elements - 1)
			firstElement = selected - nb_elements + 1;
		if (selected < firstElement)
			firstElement = selected;

		//Wallpaper
		gmenu2x->sc[((string)"skin:wallpapers/" + wallpapers[selected]).c_str()]->blit(gmenu2x->s, 0, 0);

		gmenu2x->drawTopBar(gmenu2x->s);
		gmenu2x->drawBottomBar(gmenu2x->s);

		drawTitleIcon("icons/wallpaper.png",true);
		writeTitle("Wallpaper selection");
		writeSubTitle("Select an image from the list, to use as a wallpaper");

		buttonbox.paint(gmenu2x->s, 5);

		//Selection
		iY = selected - firstElement;
		iY = top + (iY * fontheight);
		gmenu2x->s->box(2, iY, 308, fontheight, gmenu2x->skinConfColors[COLOR_SELECTION_BG]);

		//Files & Directories
		gmenu2x->s->setClipRect(0, top, 311, height);
		for (i = firstElement; i < wallpapers.size()
					&& i < firstElement + nb_elements; i++) {
			iY = i-firstElement;
			gmenu2x->s->write(gmenu2x->font, wallpapers[i], 5,
						top + (iY * fontheight),
						Font::HAlignLeft, Font::VAlignTop);
		}
		gmenu2x->s->clearClipRect();

		gmenu2x->drawScrollBar(nb_elements, wallpapers.size(), firstElement);
		gmenu2x->s->flip();

        switch(gmenu2x->input.waitForPressedButton()) {
            case InputManager::CANCEL:
                close = true;
                result = false;
                break;
            case InputManager::UP:
                if (selected == 0) selected = wallpapers.size()-1;
                else selected -= 1;
                break;
            case InputManager::ALTLEFT:
                if ((int)(selected - nb_elements + 1) < 0)
					selected = 0;
                else
					selected -= nb_elements - 1;
                break;
            case InputManager::DOWN:
                if (selected+1 >= wallpapers.size()) selected = 0;
                else selected += 1;
                break;
            case InputManager::ALTRIGHT:
                if (selected + nb_elements - 1 >= wallpapers.size())
					selected = wallpapers.size() - 1;
                else
					selected += nb_elements - 1;
                break;
            case InputManager::ACCEPT:
                close = true;
                if (wallpapers.size() > 0)
					wallpaper = gmenu2x->sc.getSkinFilePath("wallpapers/" + wallpapers[selected]);
                else result = false;
            default:
                break;
        }
	}

	for (uint i=0; i<wallpapers.size(); i++)
	  gmenu2x->sc.del("skin:wallpapers/" + wallpapers[i]);

	return result;
}
