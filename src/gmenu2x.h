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

#ifndef GMENU2X_H
#define GMENU2X_H

#include "contextmenu.h"
#include "surfacecollection.h"
#include "translator.h"
#include "touchscreen.h"
#include "inputmanager.h"
#include "surface.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Button;
class Font;
class HelpPopup;
class IconButton;
class Layer;
class LinkApp;
class MediaMonitor;
class Menu;
class Surface;

#ifndef GMENU2X_SYSTEM_DIR
#define GMENU2X_SYSTEM_DIR "/usr/share/gmenu2x"
#endif

#ifndef LOG_FILE
#define LOG_FILE "/var/log/gmenu2x.log"
#endif

const int LOOP_DELAY = 30000;

extern const char *CARD_ROOT;

// Note: Keep this in sync with colorNames!
enum color {
	COLOR_TOP_BAR_BG,
	COLOR_BOTTOM_BAR_BG,
	COLOR_SELECTION_BG,
	COLOR_MESSAGE_BOX_BG,
	COLOR_MESSAGE_BOX_BORDER,
	COLOR_MESSAGE_BOX_SELECTION,

	NUM_COLORS,
};

typedef std::unordered_map<std::string, std::string, std::hash<std::string> > ConfStrHash;
typedef std::unordered_map<std::string, int, std::hash<std::string> > ConfIntHash;

class GMenu2X {
private:
	Touchscreen ts;
	std::shared_ptr<Menu> menu;
#ifdef ENABLE_INOTIFY
	MediaMonitor *monitor;
#endif

	LinkApp *appToLaunch;
	std::string fileToLaunch;

	std::vector<std::shared_ptr<Layer>> layers;

	/*!
	Retrieves the free disk space on the sd
	@return String containing a human readable representation of the free disk space
	*/
	std::string getDiskFree(const char *path);
#ifdef ENABLE_CPUFREQ
	unsigned cpuFreqMin; //!< Minimum CPU frequency
	unsigned cpuFreqMax; //!< Maximum theoretical CPU frequency
	unsigned cpuFreqSafeMax; //!< Maximum safe CPU frequency
	unsigned cpuFreqMenuDefault; //!< Default CPU frequency for gmenu2x
	unsigned cpuFreqAppDefault; //!< Default CPU frequency for launched apps
	unsigned cpuFreqMultiple; //!< All valid CPU frequencies are a multiple of this

	void initCPULimits();
#endif
	void browsePath(const std::string &path, std::vector<std::string>* directories, std::vector<std::string>* files);

	/*!
	Displays a selector and launches the specified executable file
	*/
	void explorer();

	bool inet, //!< Represents the configuration of the basic network services. @see readCommonIni @see usbnet @see samba @see web
		usbnet,
		samba,
		web;

	std::string ip, defaultgw, lastSelectorDir;
	int lastSelectorElement;
	void readConfig();
	void readConfig(std::string path);
	void readTmp();

	void initServices();
	void initFont();
	void initMenu();
	void initBG();

public:
	GMenu2X();
	~GMenu2X();
	void quit();

	/* Returns the home directory of gmenu2x, usually
	 * ~/.gmenu2x */
	static const std::string getHome(void);

	/*
	 * Variables needed for elements disposition
	 */
	uint resX, resY, halfX, halfY;
	uint bottomBarIconY, bottomBarTextY;
	unsigned short cpuX; //!< Offset for displaying cpu clock information
	unsigned short manualX; //!< Offset for displaying the manual indicator in the taskbar

	/**
	 * Gets the position and height of the area between the top and bottom bars.
	 */
	std::pair<unsigned int, unsigned int> getContentArea() {
		const unsigned int top = skinConfInt["topBarHeight"];
		const unsigned int bottom = skinConfInt["bottomBarHeight"];
		return std::make_pair(top, resY - top - bottom);
	}

	InputManager input;

	//Configuration hashes
	ConfStrHash confStr, skinConfStr;
	ConfIntHash confInt, skinConfInt;
	RGBAColor skinConfColors[NUM_COLORS];

	//Configuration settings
	bool useSelectionPng;
	void setSkin(const std::string &skin, bool setWallpaper = true);

	SurfaceCollection sc;
	Translator tr;
	Surface *s, *bg;
	Font *font;

	//Status functions
	void main();
	void showContextMenu();
	void showHelpPopup();
	void showManual();
	void showSettings();
	void skinMenu();
	void about();
	void viewLog();
	void changeWallpaper();

#ifdef ENABLE_CPUFREQ
	void setClock(unsigned mhz);
	void setMenuClock() { setClock(cpuFreqMenuDefault); }
	void setSafeMaxClock() { setClock(cpuFreqSafeMax); }
	unsigned getDefaultAppClock() { return cpuFreqAppDefault; }
#endif

	void setInputSpeed();

	/**
	 * Requests that the given application be launched.
	 * The launch won't happen immediately; it will happen after control
	 * returns to the main loop.
	 */
	void queueLaunch(LinkApp *app, const std::string &file);
	void saveSelection();
	void writeConfig();
	void writeSkinConfig();
	void writeTmp(int selelem=-1, const std::string &selectordir="");

	void addLink();
	void editLink();
	void deleteLink();
	void addSection();
	void renameSection();
	void deleteSection();

	int drawButton(Surface *s, IconButton *btn, int x=5, int y=-10);
	int drawButton(Surface *s, const std::string &btn, const std::string &text, int x=5, int y=-10);
	int drawButtonRight(Surface *s, const std::string &btn, const std::string &text, int x=5, int y=-10);
	void drawScrollBar(uint pageSize, uint totalSize, uint pagePos);

	void drawTopBar(Surface *s);
	void drawBottomBar(Surface *s);

	Touchscreen &getTouchscreen() { return ts; }
};

#endif // GMENU2X_H
