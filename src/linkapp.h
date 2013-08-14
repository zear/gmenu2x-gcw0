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

#ifndef LINKAPP_H
#define LINKAPP_H

#include "link.h"

#include <string>

class GMenu2X;
class InputManager;
class Touchscreen;

/**
Parses links files.

	@author Massimiliano Torromeo <massimiliano.torromeo@gmail.com>
*/
class LinkApp : public Link {
private:
	InputManager &inputMgr;
	std::string sclock;
	int iclock;
	std::string exec, params, workdir, manual, selectordir, selectorfilter, selectorscreens;
	bool selectorbrowser, editable;
	void drawRun();

	std::string aliasfile;
	std::string file;

	bool dontleave;
#ifdef HAVE_LIBOPK
	bool isOPK;
	std::string opkMount, opkFile, category;
#endif

	void start();
	void launch(
			const std::string &selectedFile = "",
			const std::string &selectedDir = "");

protected:
	virtual const std::string &searchIcon();

public:
#ifdef HAVE_LIBOPK
	const std::string &getCategory() { return category; }
	bool isOpk() { return isOPK; }
	const std::string &getOpkFile() { return opkFile; }

	LinkApp(GMenu2X *gmenu2x, Touchscreen &ts, InputManager &inputMgr,
				const char* linkfile, struct OPK *opk = NULL);
#else
	LinkApp(GMenu2X *gmenu2x, Touchscreen &ts, InputManager &inputMgr,
			const char* linkfile);
	bool isOpk() { return false; }
#endif

	virtual void loadIcon();

#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0)
	bool consoleApp;
#endif

	const std::string &getExec();
	void setExec(const std::string &exec);
	const std::string &getParams();
	void setParams(const std::string &params);
	const std::string &getManual();
	void setManual(const std::string &manual);
	const std::string &getSelectorDir();
	void setSelectorDir(const std::string &selectordir);
	bool getSelectorBrowser();
	void setSelectorBrowser(bool value);
	const std::string &getSelectorScreens();
	void setSelectorScreens(const std::string &selectorscreens);
	const std::string &getSelectorFilter();
	void setSelectorFilter(const std::string &selectorfilter);
	const std::string &getAliasFile();
	void setAliasFile(const std::string &aliasfile);

	int clock();
	const std::string &clockStr(int maxClock);
	void setClock(int mhz);

	bool save();
	void showManual();
	void selector(int startSelection=0, const std::string &selectorDir="");
	bool targetExists();
	bool isEditable() { return editable; }

	const std::string &getFile() { return file; }
	void renameFile(const std::string &name);
};

#endif
