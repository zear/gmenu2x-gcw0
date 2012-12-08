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

#include "surfacecollection.h"
#include "translator.h"
#include "FastDelegate.h"
#include "touchscreen.h"
#include "inputmanager.h"
#include "surface.h"

#include <iostream>
#include <string>
#include <vector>
#include <tr1/unordered_map>

class ASFont;
class Button;
class Menu;
class Surface;

#ifndef GMENU2X_SYSTEM_DIR
#define GMENU2X_SYSTEM_DIR "/usr/share/gmenu2x"
#endif

const int LOOP_DELAY = 30000;

extern const char *CARD_ROOT;
extern const int CARD_ROOT_LEN;

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

typedef std::tr1::unordered_map<std::string, std::string, std::tr1::hash<std::string> > ConfStrHash;
typedef std::tr1::unordered_map<std::string, int, std::tr1::hash<std::string> > ConfIntHash;

class GMenu2X {
private:
	Touchscreen ts;

	/*!
	Retrieves the free disk space on the sd
	@return String containing a human readable representation of the free disk space
	*/
	std::string getDiskFree(const char *path);
	unsigned short cpuX; //!< Offset for displaying cpu clock information
	unsigned short manualX; //!< Offset for displaying the manual indicator in the taskbar
#ifdef ENABLE_CPUFREQ
	unsigned cpuFreqMin; //!< Minimum CPU frequency
	unsigned cpuFreqMax; //!< Maximum theoretical CPU frequency
	unsigned cpuFreqSafeMax; //!< Maximum safe CPU frequency
	unsigned cpuFreqMenuDefault; //!< Default CPU frequency for gmenu2x
	unsigned cpuFreqAppDefault; //!< Default CPU frequency for launched apps
	unsigned cpuFreqMultiple; //!< All valid CPU frequencies are a multiple of this

	void initCPULimits();
#endif
	/*!
	Reads the current battery state and returns a number representing it's level of charge
	@return A number representing battery charge. 0 means fully discharged. 5 means fully charged. 6 represents a gp2x using AC power.
	*/
	unsigned short getBatteryLevel();
	void browsePath(const std::string &path, std::vector<std::string>* directories, std::vector<std::string>* files);
	/*!
	Starts the scanning of the nand and sd filesystems, searching for dge and gpu files and creating the links in 2 dedicated sections.
	*/
	void scanner();
	/*!
	Performs the actual scan in the given path and populates the files vector with the results. The creation of the links is not performed here.
	@see scanner
	*/
	void scanPath(std::string path, std::vector<std::string> *files);

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

	void showManual();

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
	uint bottomBarIconY, bottomBarTextY, linkColumns, linkRows;

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
	ASFont *font;

	//Status functions
	void main();
	void options();
	void skinMenu();
	void about();
	void viewLog();
	void contextMenu();
	void changeWallpaper();

#ifdef ENABLE_CPUFREQ
	void setClock(unsigned mhz);
	void setMenuClock() { setClock(cpuFreqMenuDefault); }
	void setSafeMaxClock() { setClock(cpuFreqSafeMax); }
	unsigned getDefaultAppClock() { return cpuFreqAppDefault; }
#endif

	void setInputSpeed();

	void writeConfig();
	void writeSkinConfig();
	void writeTmp(int selelem=-1, const std::string &selectordir="");

	void addLink();
	void editLink();
	void deleteLink();
	void addSection();
	void renameSection();
	void deleteSection();

	void initBG();
	int drawButton(Button *btn, int x=5, int y=-10);
	int drawButton(Surface *s, const std::string &btn, const std::string &text, int x=5, int y=-10);
	int drawButtonRight(Surface *s, const std::string &btn, const std::string &text, int x=5, int y=-10);
	void drawScrollBar(uint pagesize, uint totalsize, uint pagepos, uint top, uint height);

	void drawTopBar(Surface *s=NULL);
	void drawBottomBar(Surface *s=NULL);

	Menu *menu;
};

#endif // GMENU2X_H
