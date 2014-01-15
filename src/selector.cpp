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

#include "selector.h"

#include "debug.h"
#include "filelister.h"
#include "gmenu2x.h"
#include "linkapp.h"
#include "menu.h"
#include "surface.h"
#include "utilities.h"

#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include <algorithm>

//for browsing the filesystem
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fstream>

using namespace std;

Selector::Selector(GMenu2X *gmenu2x, LinkApp *link, const string &selectorDir) :
	Dialog(gmenu2x)
{
	this->link = link;
	loadAliases();
	selRow = 0;
	if (selectorDir.empty())
		dir = link->getSelectorDir();
	else
		dir = selectorDir;
	if (dir[dir.length()-1]!='/') dir += "/";
}

int Selector::exec(int startSelection) {
	bool close = false, result = true;
	vector<string> screens, titles;

	FileLister fl(dir, link->getSelectorBrowser());
	fl.setFilter(link->getSelectorFilter());
	fl.browse();

	Surface bg(gmenu2x->bg);
	drawTitleIcon(link->getIconPath(), true, &bg);
	writeTitle(link->getTitle(), &bg);
	writeSubTitle(link->getDescription(), &bg);

	if (link->getSelectorBrowser()) {
		gmenu2x->drawButton(&bg, "start", gmenu2x->tr["Exit"],
		gmenu2x->drawButton(&bg, "accept", gmenu2x->tr["Select a file"],
		gmenu2x->drawButton(&bg, "cancel", gmenu2x->tr["Up one folder"],
		gmenu2x->drawButton(&bg, "left", "", 5)-10)));
	} else {
		gmenu2x->drawButton(&bg, "start", gmenu2x->tr["Exit"],
		gmenu2x->drawButton(&bg, "cancel", "",
		gmenu2x->drawButton(&bg, "accept", gmenu2x->tr["Select a file"], 5)) - 10);
	}

	unsigned int top, height;
	tie(top, height) = gmenu2x->getContentArea();

	int fontheight = gmenu2x->font->getHeight();
	unsigned int nb_elements = height / fontheight;

	bg.convertToDisplayFormat();

	Uint32 selTick = SDL_GetTicks(), curTick;
	uint i, firstElement = 0, iY;

	prepare(&fl,&screens,&titles);
	uint selected = constrain(startSelection,0,fl.size()-1);

	//Add the folder icon manually to be sure to load it with alpha support since we are going to disable it for screenshots
	if (gmenu2x->sc.skinRes("imgs/folder.png")==NULL)
		gmenu2x->sc.addSkinRes("imgs/folder.png");
	gmenu2x->sc.defaultAlpha = false;
	while (!close) {
		bg.blit(gmenu2x->s,0,0);

		if (selected >= firstElement + nb_elements)
			firstElement = selected - nb_elements + 1;
		if (selected < firstElement)
			firstElement = selected;

		//Selection
		iY = top + (selected - firstElement) * fontheight;
		if (selected<fl.size())
			gmenu2x->s->box(1, iY, 309, fontheight, gmenu2x->skinConfColors[COLOR_SELECTION_BG]);

		//Screenshot
		if (selected-fl.dirCount()<screens.size()
				&& !screens[selected-fl.dirCount()].empty()) {
			curTick = SDL_GetTicks();
			if (curTick - selTick > 200) {
				gmenu2x->sc[screens[selected-fl.dirCount()]]->blitRight(
						gmenu2x->s, 311, 42, 160, 160,
						min((curTick - selTick - 200) / 3, 255u));
			}
		}

		//Files & Dirs
		gmenu2x->s->setClipRect(0, top, 311, height);
		for (i = firstElement; i < fl.size()
					&& i < firstElement + nb_elements; i++) {
			iY = i-firstElement;
			if (fl.isDirectory(i)) {
				gmenu2x->sc["imgs/folder.png"]->blit(gmenu2x->s, 4, top + (iY * fontheight));
				gmenu2x->s->write(gmenu2x->font, fl[i], 21, top+(iY * fontheight), Font::HAlignLeft, Font::VAlignMiddle);
			} else
				gmenu2x->s->write(gmenu2x->font, titles[i - fl.dirCount()], 4,
							top + (iY * fontheight),
							Font::HAlignLeft, Font::VAlignTop);
		}
		gmenu2x->s->clearClipRect();

		gmenu2x->drawScrollBar(nb_elements, fl.size(), firstElement);
		gmenu2x->s->flip();

		switch (gmenu2x->input.waitForPressedButton()) {
			case InputManager::SETTINGS:
				close = true;
				result = false;
				break;

			case InputManager::UP:
				if (selected == 0) selected = fl.size() -1;
				else selected -= 1;
				selTick = SDL_GetTicks();
				break;

			case InputManager::ALTLEFT:
				if ((int)(selected - nb_elements + 1) < 0)
					selected = 0;
				else
					selected -= nb_elements - 1;
				selTick = SDL_GetTicks();
				break;

			case InputManager::DOWN:
				if (selected+1>=fl.size()) selected = 0;
				else selected += 1;
				selTick = SDL_GetTicks();
				break;

			case InputManager::ALTRIGHT:
				if (selected + nb_elements - 1 >= fl.size())
					selected = fl.size() - 1;
				else
					selected += nb_elements - 1;
				selTick = SDL_GetTicks();
				break;

			case InputManager::CANCEL:
				if (!link->getSelectorBrowser()) {
					close = true;
					result = false;
					break;
				}

			case InputManager::LEFT:
				if (link->getSelectorBrowser()) {
					string::size_type p = dir.rfind("/", dir.size()-2);
					if (p==string::npos || dir.compare(0, 1, "/") != 0 || dir.length() < 2) {
						close = true;
						result = false;
					} else {
						dir = dir.substr(0,p+1);
						selected = 0;
						firstElement = 0;
						prepare(&fl,&screens,&titles);
					}
				}
				break;

			case InputManager::ACCEPT:
				if (fl.isFile(selected)) {
					file = fl[selected];
					close = true;
				} else {
					dir = dir+fl[selected];
					char *buf = realpath(dir.c_str(), NULL);
					dir = (string) buf + '/';
					free(buf);

					selected = 0;
					firstElement = 0;
					prepare(&fl,&screens,&titles);
				}
				break;

			default:
				break;
		}
	}

	gmenu2x->sc.defaultAlpha = true;
	freeScreenshots(&screens);

	return result ? (int)selected : -1;
}

void Selector::prepare(FileLister *fl, vector<string> *screens, vector<string> *titles) {
	fl->setPath(dir);
	freeScreenshots(screens);
	screens->resize(fl->getFiles().size());
	titles->resize(fl->getFiles().size());

	string screendir = link->getSelectorScreens();
	if (!screendir.empty() && screendir[screendir.length() - 1] != '/') {
		screendir += "/";
	}

	string noext;
	string::size_type pos;
	for (uint i=0; i<fl->getFiles().size(); i++) {
		noext = fl->getFiles()[i];
		pos = noext.rfind(".");
		if (pos!=string::npos && pos>0)
			noext = noext.substr(0, pos);
		titles->at(i) = getAlias(noext);
		if (titles->at(i).empty())
			titles->at(i) = noext;

		DEBUG("Searching for screen '%s%s.png'\n", screendir.c_str(), noext.c_str());

		if (fileExists(screendir+noext+".png"))
			screens->at(i) = screendir+noext+".png";
		else
			screens->at(i) = "";
	}
}

void Selector::freeScreenshots(vector<string> *screens) {
	for (uint i=0; i<screens->size(); i++) {
		if (!screens->at(i).empty())
			gmenu2x->sc.del(screens->at(i));
	}
}

void Selector::loadAliases() {
	aliases.clear();
	if (fileExists(link->getAliasFile())) {
		string line;
		ifstream infile (link->getAliasFile().c_str(), ios_base::in);
		while (getline(infile, line, '\n')) {
			string::size_type position = line.find("=");
			string name = trim(line.substr(0,position));
			string value = trim(line.substr(position+1));
			aliases[name] = value;
		}
		infile.close();
	}
}

string Selector::getAlias(const string &key) {
	unordered_map<string, string>::iterator i = aliases.find(key);
	if (i == aliases.end())
		return "";
	else
		return i->second;
}
