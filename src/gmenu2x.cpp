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

#include "gp2x.h"

#include "clock.h"
#include "cpu.h"
#include "debug.h"
#include "filedialog.h"
#include "filelister.h"
#include "font.h"
#include "gmenu2x.h"
#include "iconbutton.h"
#include "inputdialog.h"
#include "linkapp.h"
#include "mediamonitor.h"
#include "menu.h"
#include "menusettingbool.h"
#include "menusettingdir.h"
#include "menusettingfile.h"
#include "menusettingimage.h"
#include "menusettingint.h"
#include "menusettingmultistring.h"
#include "menusettingrgba.h"
#include "menusettingstring.h"
#include "messagebox.h"
#include "powersaver.h"
#include "settingsdialog.h"
#include "textdialog.h"
#include "wallpaperdialog.h"
#include "utilities.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include <signal.h>

#include <sys/statvfs.h>
#include <errno.h>

#include <sys/fcntl.h> //for battery

#ifdef PLATFORM_PANDORA
//#include <pnd_container.h>
//#include <pnd_conf.h>
//#include <pnd_discovery.h>
#endif

//for browsing the filesystem
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

//for soundcard
#include <sys/ioctl.h>
#include <linux/soundcard.h>

#include <sys/mman.h>

struct MenuOption {
	std::string text;
	function_t action;
};

using namespace std;

#ifndef DEFAULT_WALLPAPER_PATH
#define DEFAULT_WALLPAPER_PATH \
  GMENU2X_SYSTEM_DIR "/skins/Default/wallpapers/default.png"
#endif

#ifdef _CARD_ROOT
const char *CARD_ROOT = _CARD_ROOT;
#elif defined(PLATFORM_A320) || defined(PLATFORM_GCW0)
const char *CARD_ROOT = "/media";
#else
const char *CARD_ROOT = "/card";
#endif

static GMenu2X *app;
static string gmenu2x_home;

// Note: Keep this in sync with the enum!
static const char *colorNames[NUM_COLORS] = {
	"topBarBg",
	"bottomBarBg",
	"selectionBg",
	"messageBoxBg",
	"messageBoxBorder",
	"messageBoxSelection",
};

static enum color stringToColor(const string &name)
{
	for (unsigned int i = 0; i < NUM_COLORS; i++) {
		if (strcmp(colorNames[i], name.c_str()) == 0) {
			return (enum color)i;
		}
	}
	return (enum color)-1;
}

static const char *colorToString(enum color c)
{
	return colorNames[c];
}

static void quit_all(int err) {
    delete app;
    exit(err);
}

const string GMenu2X::getHome(void)
{
	return gmenu2x_home;
}

static void set_handler(int signal, void (*handler)(int))
{
	struct sigaction sig;
	sigaction(signal, NULL, &sig);
	sig.sa_handler = handler;
	sigaction(signal, &sig, NULL);
}

int main(int /*argc*/, char * /*argv*/[]) {
	INFO("---- GMenu2X starting ----\n");

	set_handler(SIGINT, &quit_all);
	set_handler(SIGSEGV, &quit_all);
	set_handler(SIGTERM, &quit_all);

	char *home = getenv("HOME");
	if (home == NULL) {
		ERROR("Unable to find gmenu2x home directory. The $HOME variable is not defined.\n");
		return 1;
	}

	gmenu2x_home = (string)home + (string)"/.gmenu2x";
	if (!fileExists(gmenu2x_home) && mkdir(gmenu2x_home.c_str(), 0770) < 0) {
		ERROR("Unable to create gmenu2x home directory.\n");
		return 1;
	}

	DEBUG("Home path: %s.\n", gmenu2x_home.c_str());

	app = new GMenu2X();
	DEBUG("Starting main()\n");
	app->main();

	return 0;
}

#ifdef ENABLE_CPUFREQ
void GMenu2X::initCPULimits() {
	// Note: These values are for the Dingoo.
	//       The NanoNote does not have cpufreq enabled in its kernel and
	//       other devices are not actively maintained.
	// TODO: Read min and max from sysfs.
	cpuFreqMin = 30;
	cpuFreqMax = 500;
	cpuFreqSafeMax = 420;
	cpuFreqMenuDefault = 200;
	cpuFreqAppDefault = 384;
	cpuFreqMultiple = 24;

	// Round min and max values to the specified multiple.
	cpuFreqMin = ((cpuFreqMin + cpuFreqMultiple - 1) / cpuFreqMultiple)
			* cpuFreqMultiple;
	cpuFreqMax = (cpuFreqMax / cpuFreqMultiple) * cpuFreqMultiple;
	cpuFreqSafeMax = (cpuFreqSafeMax / cpuFreqMultiple) * cpuFreqMultiple;
	cpuFreqMenuDefault = (cpuFreqMenuDefault / cpuFreqMultiple) * cpuFreqMultiple;
	cpuFreqAppDefault = (cpuFreqAppDefault / cpuFreqMultiple) * cpuFreqMultiple;
}
#endif

GMenu2X::GMenu2X()
{
	usbnet = samba = inet = web = false;
	useSelectionPng = false;

#ifdef ENABLE_CPUFREQ
	initCPULimits();
#endif
	//load config data
	readConfig();

	halfX = resX/2;
	halfY = resY/2;
	bottomBarIconY = resY-18;
	bottomBarTextY = resY-10;

	/* Do not clear the screen on exit.
	 * This may require an SDL patch available at
	 * https://github.com/mthuurne/opendingux-buildroot/blob
	 * 			/opendingux-2010.11/package/sdl/sdl-fbcon-clear-onexit.patch
	 */
	setenv("SDL_FBCON_DONT_CLEAR", "1", 0);

	//Screen
	if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		ERROR("Could not initialize SDL: %s\n", SDL_GetError());
		quit();
	}

	s = Surface::openOutputSurface(resX, resY, confInt["videoBpp"]);

	bg = NULL;
	font = NULL;
	menu = NULL;
	setSkin(confStr["skin"], !fileExists(confStr["wallpaper"]));
	initMenu();

	monitor = new MediaMonitor(CARD_ROOT);

	if (!fileExists(confStr["wallpaper"])) {
		DEBUG("No wallpaper defined; we will take the default one.\n");
		confStr["wallpaper"] = DEFAULT_WALLPAPER_PATH;
	}

	initBG();

	/* If a user-specified input.conf file exists, we load it;
	 * otherwise, we load the default one. */
	string input_file = getHome() + "/input.conf";
	if (fileExists(input_file.c_str())) {
		DEBUG("Loading user-specific input.conf file: %s.\n", input_file.c_str());
	} else {
		input_file = GMENU2X_SYSTEM_DIR "/input.conf";
		DEBUG("Loading system input.conf file: %s.\n", input_file.c_str());
	}

	input.init(input_file, menu);

	if (confInt["backlightTimeout"] > 0)
        PowerSaver::getInstance()->setScreenTimeout( confInt["backlightTimeout"] );

	setInputSpeed();
#ifdef ENABLE_CPUFREQ
	setClock(confInt["menuClock"]);
#endif
	//recover last session
	readTmp();
	if (lastSelectorElement>-1 && menu->selLinkApp()!=NULL && (!menu->selLinkApp()->getSelectorDir().empty() || !lastSelectorDir.empty()))
		menu->selLinkApp()->selector(lastSelectorElement,lastSelectorDir);

}

GMenu2X::~GMenu2X() {
	if (PowerSaver::isRunning())
		delete PowerSaver::getInstance();
	if (Clock::isRunning())
		delete Clock::getInstance();
	quit();

	delete menu;
	delete font;
	delete monitor;
}

void GMenu2X::quit() {
	fflush(NULL);
	sc.clear();
	delete s;

	SDL_Quit();
	unsetenv("SDL_FBCON_DONT_CLEAR");
}

void GMenu2X::initBG() {
	sc.del("bgmain");

	// Load wallpaper.
	delete bg;
	bg = Surface::loadImage(confStr["wallpaper"]);
	if (!bg) {
		bg = Surface::emptySurface(resX, resY);
	}

	drawTopBar(bg);
	drawBottomBar(bg);

	Surface *bgmain = new Surface(bg);
	sc.add(bgmain,"bgmain");

	Surface *sd = Surface::loadImage("imgs/sd.png", confStr["skin"]);
	if (sd) sd->blit(bgmain, 3, bottomBarIconY);

#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0)
	string df = getDiskFree("/boot");
#else
	string df = getDiskFree(CARD_ROOT);
#endif
	bgmain->write(font, df, 22, bottomBarTextY, Font::HAlignLeft, Font::VAlignMiddle);
	delete sd;

	cpuX = font->getTextWidth(df)+32;
#ifdef ENABLE_CPUFREQ
	Surface *cpu = Surface::loadImage("imgs/cpu.png", confStr["skin"]);
	if (cpu) cpu->blit(bgmain, cpuX, bottomBarIconY);
	cpuX += 19;
	manualX = cpuX+font->getTextWidth("300MHz")+5;
	delete cpu;
#else
	manualX = cpuX;
#endif

	int serviceX = resX-38;
	if (usbnet) {
		if (web) {
			Surface *webserver = Surface::loadImage(
				"imgs/webserver.png", confStr["skin"]);
			if (webserver) webserver->blit(bgmain, serviceX, bottomBarIconY);
			serviceX -= 19;
			delete webserver;
		}
		if (samba) {
			Surface *sambaS = Surface::loadImage(
				"imgs/samba.png", confStr["skin"]);
			if (sambaS) sambaS->blit(bgmain, serviceX, bottomBarIconY);
			serviceX -= 19;
			delete sambaS;
		}
		if (inet) {
			Surface *inetS = Surface::loadImage("imgs/inet.png", confStr["skin"]);
			if (inetS) inetS->blit(bgmain, serviceX, bottomBarIconY);
			serviceX -= 19;
			delete inetS;
		}
	}

	bgmain->convertToDisplayFormat();
}

void GMenu2X::initFont() {
	delete font;
	font = Font::defaultFont();
	if (!font) {
		ERROR("Cannot function without font; aborting...\n");
		quit();
		exit(-1);
	}
}

void GMenu2X::initMenu() {
	//Menu structure handler
	menu = new Menu(this, ts);
	for (uint i=0; i<menu->getSections().size(); i++) {
		//Add virtual links in the applications section
		if (menu->getSections()[i]=="applications") {
			menu->addActionLink(i,"Explorer", BIND(&GMenu2X::explorer),tr["Launch an application"],"skin:icons/explorer.png");
		}

		//Add virtual links in the setting section
		else if (menu->getSections()[i]=="settings") {
			menu->addActionLink(i,"GMenu2X",BIND(&GMenu2X::options),tr["Configure GMenu2X's options"],"skin:icons/configure.png");
			menu->addActionLink(i,tr["Skin"],BIND(&GMenu2X::skinMenu),tr["Configure skin"],"skin:icons/skin.png");
			menu->addActionLink(i,tr["Wallpaper"],BIND(&GMenu2X::changeWallpaper),tr["Change GMenu2X wallpaper"],"skin:icons/wallpaper.png");
			if (fileExists(getHome()+"/log.txt"))
				menu->addActionLink(i,tr["Log Viewer"],BIND(&GMenu2X::viewLog),tr["Displays last launched program's output"],"skin:icons/ebook.png");
			menu->addActionLink(i,tr["About"],BIND(&GMenu2X::about),tr["Info about GMenu2X"],"skin:icons/about.png");
		}
	}

	menu->setSectionIndex(confInt["section"]);
	menu->setLinkIndex(confInt["link"]);

	menu->loadIcons();

	//DEBUG
	//menu->addLink( CARD_ROOT, "sample.pxml", "applications" );
}

void GMenu2X::about() {
	vector<string> text;
	string line;
	string fn(GMENU2X_SYSTEM_DIR);
	string build_date("Build date: ");
	fn.append("/about.txt");
	build_date.append(__DATE__);

	ifstream inf(fn.c_str(), ios_base::in);

	while(getline(inf, line, '\n'))
		text.push_back(line);
	inf.close();

	TextDialog td(this, "GMenu2X", build_date, "icons/about.png", &text);
	td.exec();
}

void GMenu2X::viewLog() {
	string logfile = getHome()+"/log.txt";
	if (fileExists(logfile)) {
		ifstream inf(logfile.c_str(), ios_base::in);
		if (inf.is_open()) {
			vector<string> log;

			string line;
			while (getline(inf, line, '\n'))
				log.push_back(line);
			inf.close();

			TextDialog td(this, tr["Log Viewer"], tr["Displays last launched program's output"], "icons/ebook.png", &log);
			td.exec();

			MessageBox mb(this, tr["Do you want to delete the log file?"], "icons/ebook.png");
			mb.setButton(InputManager::ACCEPT, tr["Yes"]);
			mb.setButton(InputManager::CANCEL, tr["No"]);
			if (mb.exec() == InputManager::ACCEPT) {
				unlink(logfile.c_str());
				sync();
				menu->deleteSelectedLink();
			}
		}
	}
}

void GMenu2X::readConfig() {
	string conffile = GMENU2X_SYSTEM_DIR "/gmenu2x.conf";
	readConfig(conffile);

	conffile = getHome() + "/gmenu2x.conf";
	readConfig(conffile);
}

void GMenu2X::readConfig(string conffile) {
	if (fileExists(conffile)) {
		ifstream inf(conffile.c_str(), ios_base::in);
		if (inf.is_open()) {
			string line;
			while (getline(inf, line, '\n')) {
				string::size_type pos = line.find("=");
				string name = trim(line.substr(0,pos));
				string value = trim(line.substr(pos+1,line.length()));

				if (value.length()>1 && value.at(0)=='"' && value.at(value.length()-1)=='"')
					confStr[name] = value.substr(1,value.length()-2);
				else
					confInt[name] = atoi(value.c_str());
			}
			inf.close();
		}
	}
	if (!confStr["lang"].empty())
		tr.setLang(confStr["lang"]);

	if (!confStr["wallpaper"].empty() && !fileExists(confStr["wallpaper"]))
		confStr["wallpaper"] = "";

	if (confStr["skin"].empty() || SurfaceCollection::getSkinPath(confStr["skin"]).empty())
		confStr["skin"] = "Default";

	evalIntConf( &confInt["outputLogs"], 0, 0,1 );
#ifdef ENABLE_CPUFREQ
	evalIntConf( &confInt["maxClock"],
				 cpuFreqSafeMax, cpuFreqMin, cpuFreqMax );
	evalIntConf( &confInt["menuClock"],
				 cpuFreqMenuDefault, cpuFreqMin, cpuFreqSafeMax );
#endif
	evalIntConf( &confInt["backlightTimeout"], 15, 0,120 );
	evalIntConf( &confInt["videoBpp"], 32, 16, 32 );

	if (confStr["tvoutEncoding"] != "PAL") confStr["tvoutEncoding"] = "NTSC";
	resX = constrain( confInt["resolutionX"], 320,1920 );
	resY = constrain( confInt["resolutionY"], 240,1200 );
}

void GMenu2X::saveSelection() {
	if (confInt["saveSelection"] && (
			confInt["section"] != menu->selSectionIndex()
			|| confInt["link"] != menu->selLinkIndex()
	)) {
		writeConfig();
	}
}

void GMenu2X::writeConfig() {
	string conffile = getHome() + "/gmenu2x.conf";
	ofstream inf(conffile.c_str());
	if (inf.is_open()) {
		ConfStrHash::iterator endS = confStr.end();
		for(ConfStrHash::iterator curr = confStr.begin(); curr != endS; curr++)
			inf << curr->first << "=\"" << curr->second << "\"" << endl;

		ConfIntHash::iterator endI = confInt.end();
		for(ConfIntHash::iterator curr = confInt.begin(); curr != endI; curr++)
			inf << curr->first << "=" << curr->second << endl;

		inf.close();
		sync();
	}
}

void GMenu2X::writeSkinConfig() {
	string conffile = getHome() + "/skins/";
	if (!fileExists(conffile))
	  mkdir(conffile.c_str(), 0770);
	conffile = conffile + confStr["skin"];
	if (!fileExists(conffile))
	  mkdir(conffile.c_str(), 0770);
	conffile = conffile + "/skin.conf";

	ofstream inf(conffile.c_str());
	if (inf.is_open()) {
		ConfStrHash::iterator endS = skinConfStr.end();
		for(ConfStrHash::iterator curr = skinConfStr.begin(); curr != endS; curr++)
			inf << curr->first << "=\"" << curr->second << "\"" << endl;

		ConfIntHash::iterator endI = skinConfInt.end();
		for(ConfIntHash::iterator curr = skinConfInt.begin(); curr != endI; curr++)
			inf << curr->first << "=" << curr->second << endl;

		int i;
		for (i = 0; i < NUM_COLORS; ++i) {
			inf << colorToString((enum color)i) << "=#";
			inf.width(2); inf.fill('0');
			inf << right << hex << skinConfColors[i].r;
			inf.width(2); inf.fill('0');
			inf << right << hex << skinConfColors[i].g;
			inf.width(2); inf.fill('0');
			inf << right << hex << skinConfColors[i].b;
			inf.width(2); inf.fill('0');
			inf << right << hex << skinConfColors[i].a << endl;
		}

		inf.close();
		sync();
	}
}

void GMenu2X::readTmp() {
	lastSelectorElement = -1;
	if (fileExists("/tmp/gmenu2x.tmp")) {
		ifstream inf("/tmp/gmenu2x.tmp", ios_base::in);
		if (inf.is_open()) {
			string line;
			string section = "";
			while (getline(inf, line, '\n')) {
				string::size_type pos = line.find("=");
				string name = trim(line.substr(0,pos));
				string value = trim(line.substr(pos+1,line.length()));

				if (name=="section")
					menu->setSectionIndex(atoi(value.c_str()));
				else if (name=="link")
					menu->setLinkIndex(atoi(value.c_str()));
				else if (name=="selectorelem")
					lastSelectorElement = atoi(value.c_str());
				else if (name=="selectordir")
					lastSelectorDir = value;
			}
			inf.close();
		}
	}
}

void GMenu2X::writeTmp(int selelem, const string &selectordir) {
	string conffile = "/tmp/gmenu2x.tmp";
	ofstream inf(conffile.c_str());
	if (inf.is_open()) {
		inf << "section=" << menu->selSectionIndex() << endl;
		inf << "link=" << menu->selLinkIndex() << endl;
		if (selelem>-1)
			inf << "selectorelem=" << selelem << endl;
		if (selectordir!="")
			inf << "selectordir=" << selectordir << endl;
		inf.close();
		sync();
	}
}

void GMenu2X::main() {
	uint linksPerPage = linkColumns*linkRows;
	int linkSpacingX = (resX-10 - linkColumns*skinConfInt["linkWidth"])/linkColumns;
	int linkSpacingY = (resY-35 - skinConfInt["topBarHeight"] - linkRows*skinConfInt["linkHeight"])/linkRows;
	uint sectionLinkPadding = (skinConfInt["topBarHeight"] - 32 - font->getHeight()) / 3;

	bool quit = false;
	int x,y;
	int helpBoxHeight = 154;
	uint i;
	long tickBattery = -60000, tickNow;
	string batteryIcon = "imgs/battery/0.png";
	stringstream ss;
	uint sectionsCoordX = 24;
	SDL_Rect re = {0,0,0,0};
	bool helpDisplayed = false;
#ifdef WITH_DEBUG
	//framerate
	long tickFPS = SDL_GetTicks();
	int drawn_frames = 0;
	string fps = "";
#endif

	IconButton btnContextMenu(this, ts, "skin:imgs/menu.png");
	btnContextMenu.setPosition(resX-38, bottomBarIconY);
	btnContextMenu.setAction(BIND(&GMenu2X::contextMenu));

	if (!fileExists(CARD_ROOT))
		CARD_ROOT = "";

	while (!quit) {
		tickNow = SDL_GetTicks();

		//Background
		sc["bgmain"]->blit(s,0,0);

		//Sections
		sectionsCoordX = halfX - (constrain((uint)menu->getSections().size(), 0 , linkColumns) * skinConfInt["linkWidth"]) / 2;
		if (menu->firstDispSection()>0)
			sc.skinRes("imgs/l_enabled.png")->blit(s,0,0);
		else
			sc.skinRes("imgs/l_disabled.png")->blit(s,0,0);
		if (menu->firstDispSection()+linkColumns<menu->getSections().size())
			sc.skinRes("imgs/r_enabled.png")->blit(s,resX-10,0);
		else
			sc.skinRes("imgs/r_disabled.png")->blit(s,resX-10,0);
		for (i=menu->firstDispSection(); i<menu->getSections().size() && i<menu->firstDispSection()+linkColumns; i++) {
			string sectionIcon = "skin:sections/"+menu->getSections()[i]+".png";
			x = (i-menu->firstDispSection())*skinConfInt["linkWidth"]+sectionsCoordX;
			if (menu->selSectionIndex()==(int)i)
				s->box(x, 0, skinConfInt["linkWidth"],
				skinConfInt["topBarHeight"], skinConfColors[COLOR_SELECTION_BG]);
			x += skinConfInt["linkWidth"]/2;
			if (sc.exists(sectionIcon))
				sc[sectionIcon]->blit(s,x-16,sectionLinkPadding,32,32);
			else
				sc.skinRes("icons/section.png")->blit(s,x-16,sectionLinkPadding);
			s->write( font, menu->getSections()[i], x, skinConfInt["topBarHeight"]-sectionLinkPadding, Font::HAlignCenter, Font::VAlignBottom );
		}

		//Links
		for (i=menu->firstDispRow()*linkColumns; i<(menu->firstDispRow()*linkColumns)+linksPerPage && i<menu->sectionLinks()->size(); i++) {
			int ir = i-menu->firstDispRow()*linkColumns;
			x = (ir%linkColumns)*(skinConfInt["linkWidth"]+linkSpacingX)+6;
			y = ir/linkColumns*(skinConfInt["linkHeight"]+linkSpacingY)+skinConfInt["topBarHeight"]+2;
			menu->sectionLinks()->at(i)->setPosition(x,y);

			if (i==(uint)menu->selLinkIndex())
				menu->sectionLinks()->at(i)->paintHover();

			menu->sectionLinks()->at(i)->paint();
		}
		s->clearClipRect();

		drawScrollBar(linkRows,menu->sectionLinks()->size()/linkColumns + ((menu->sectionLinks()->size()%linkColumns==0) ? 0 : 1),menu->firstDispRow(),43,resY-81);

		if (menu->selLink()!=NULL) {
			s->write ( font, menu->selLink()->getDescription(), halfX, resY-19, Font::HAlignCenter, Font::VAlignBottom );
			if (menu->selLinkApp()!=NULL) {
#ifdef ENABLE_CPUFREQ
				s->write ( font, menu->selLinkApp()->clockStr(confInt["maxClock"]), cpuX, bottomBarTextY, Font::HAlignLeft, Font::VAlignMiddle );
#endif
				//Manual indicator
				if (!menu->selLinkApp()->getManual().empty())
					sc.skinRes("imgs/manual.png")->blit(s,manualX,bottomBarIconY);
			}
		}

		if (ts.available()) {
			btnContextMenu.paint();
		}
		//check battery status every 60 seconds
		if (tickNow-tickBattery >= 60000) {
			tickBattery = tickNow;
			unsigned short battlevel = getBatteryLevel();
			if (battlevel>5) {
				batteryIcon = "imgs/battery/ac.png";
			} else {
				ss.clear();
				ss << battlevel;
				ss >> batteryIcon;
				batteryIcon = "imgs/battery/"+batteryIcon+".png";
			}
		}
		sc.skinRes(batteryIcon)->blit( s, resX-19, bottomBarIconY );
		//s->write( font, tr[batstr.c_str()], 20, 170 );
		//On Screen Help

		s->write(font, Clock::getInstance()->getTime(),
					halfX, bottomBarTextY,
					Font::HAlignCenter, Font::VAlignMiddle);

		if (helpDisplayed) {
			s->box(10,50,300,helpBoxHeight+4, skinConfColors[COLOR_MESSAGE_BOX_BG]);
			s->rectangle( 12,52,296,helpBoxHeight,
			skinConfColors[COLOR_MESSAGE_BOX_BORDER] );
			s->write( font, tr["CONTROLS"], 20, 60 );
#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0)
			s->write( font, tr["A: Launch link / Confirm action"], 20, 80 );
			s->write( font, tr["B: Show this help menu"], 20, 95 );
			s->write( font, tr["L, R: Change section"], 20, 110 );
			s->write( font, tr["SELECT: Show contextual menu"], 20, 155 );
			s->write( font, tr["START: Show options menu"], 20, 170 );
#endif
			s->flip();
			while (input.waitForPressedButton() != InputManager::CANCEL) {}
			helpDisplayed=false;
			continue;
		}

#ifdef WITH_DEBUG
		//framerate
		drawn_frames++;
		if (tickNow-tickFPS>=1000) {
			ss.clear();
			ss << drawn_frames*(tickNow-tickFPS+1)/1000;
			ss >> fps;
			tickFPS = tickNow;
			drawn_frames = 0;
		}
		s->write(font, fps + " FPS", resX - 1, 1, Font::HAlignRight);
#endif

		s->flip();

		//touchscreen
		if (ts.available()) {
			ts.poll();
			btnContextMenu.handleTS();
			re.x = 0; re.y = 0; re.h = skinConfInt["topBarHeight"]; re.w = resX;
			if (ts.pressed() && ts.inRect(re)) {
				re.w = skinConfInt["linkWidth"];
				sectionsCoordX = halfX - (constrain((uint)menu->getSections().size(), 0 , linkColumns) * skinConfInt["linkWidth"]) / 2;
				for (i=menu->firstDispSection(); !ts.handled() && i<menu->getSections().size() && i<menu->firstDispSection()+linkColumns; i++) {
					re.x = (i-menu->firstDispSection())*re.w+sectionsCoordX;

					if (ts.inRect(re)) {
						menu->setSectionIndex(i);
						ts.setHandled();
					}
				}
			}

			i=menu->firstDispRow()*linkColumns;
			while ( i<(menu->firstDispRow()*linkColumns)+linksPerPage && i<menu->sectionLinks()->size()) {
				if (menu->sectionLinks()->at(i)->isPressed())
					menu->setLinkIndex(i);
				if (menu->sectionLinks()->at(i)->handleTS())
					i = menu->sectionLinks()->size();
				i++;
			}
		}

        switch (input.waitForPressedButton()) {
            case InputManager::ACCEPT:
                if (menu->selLink() != NULL) menu->selLink()->run();
                break;
            case InputManager::CANCEL:
                helpDisplayed=true;
                break;
            case InputManager::SETTINGS:
                options();
                break;
            case InputManager::MENU:
                contextMenu();
                break;
            case InputManager::UP:
                menu->linkUp();
                break;
            case InputManager::DOWN:
                menu->linkDown();
                break;
            case InputManager::LEFT:
                menu->linkLeft();
                break;
            case InputManager::RIGHT:
                menu->linkRight();
                break;
            case InputManager::ALTLEFT:
				menu->decSectionIndex();
                break;
            case InputManager::ALTRIGHT:
				menu->incSectionIndex();
                break;
            default:
                break;
        }
	}
}

void GMenu2X::explorer() {
	FileDialog fd(this, ts, tr["Select an application"], "dge,sh,bin,py,elf,");
	if (fd.exec()) {
		if (confInt["saveSelection"] && (confInt["section"]!=menu->selSectionIndex() || confInt["link"]!=menu->selLinkIndex()))
			writeConfig();

		string command = cmdclean(fd.getPath()+"/"+fd.getFile());
		chdir(fd.getPath().c_str());
		quit();
#ifdef ENABLE_CPUFREQ
		setClock(cpuFreqAppDefault);
#endif
		execlp("/bin/sh","/bin/sh","-c",command.c_str(),NULL);

		//if execution continues then something went wrong and as we already called SDL_Quit we cannot continue
		//try relaunching gmenu2x
		ERROR("Error executing selected application, re-launching gmenu2x\n");
		main();
	}
}

void GMenu2X::options() {
#ifdef ENABLE_CPUFREQ
	int curMenuClock = confInt["menuClock"];
#endif
	bool showRootFolder = fileExists(CARD_ROOT);

	FileLister fl_tr(GMENU2X_SYSTEM_DIR "/translations");
	fl_tr.browse();

	string tr_path = getHome() + "/translations";
	if (fileExists(tr_path)) {
		fl_tr.setPath(tr_path, false);
		fl_tr.browse(false);
	}

	fl_tr.insertFile("English");
	string lang = tr.lang();

	vector<string> encodings;
	encodings.push_back("NTSC");
	encodings.push_back("PAL");

	SettingsDialog sd(this, input, ts, tr["Settings"]);
	sd.addSetting(new MenuSettingMultiString(this, ts, tr["Language"], tr["Set the language used by GMenu2X"], &lang, &fl_tr.getFiles()));
	sd.addSetting(new MenuSettingBool(this, ts, tr["Save last selection"], tr["Save the last selected link and section on exit"], &confInt["saveSelection"]));
#ifdef ENABLE_CPUFREQ
	sd.addSetting(new MenuSettingInt(this, ts, tr["Clock for GMenu2X"], tr["Set the cpu working frequency when running GMenu2X"], &confInt["menuClock"], cpuFreqMin, cpuFreqSafeMax, cpuFreqMultiple));
	sd.addSetting(new MenuSettingInt(this, ts, tr["Maximum overclock"], tr["Set the maximum overclock for launching links"], &confInt["maxClock"], cpuFreqMin, cpuFreqMax, cpuFreqMultiple));
#endif
	sd.addSetting(new MenuSettingBool(this, ts, tr["Output logs"], tr["Logs the output of the links. Use the Log Viewer to read them."], &confInt["outputLogs"]));
	sd.addSetting(new MenuSettingInt(this, ts, tr["Screen Timeout"], tr["Set screen's backlight timeout in seconds"], &confInt["backlightTimeout"], 0, 120));
//	sd.addSetting(new MenuSettingMultiString(this, ts, tr["Tv-Out encoding"], tr["Encoding of the tv-out signal"], &confStr["tvoutEncoding"], &encodings));
	sd.addSetting(new MenuSettingBool(this, ts, tr["Show root"], tr["Show root folder in the file selection dialogs"], &showRootFolder));

	if (sd.exec() && sd.edited()) {
#ifdef ENABLE_CPUFREQ
		if (curMenuClock != confInt["menuClock"]) setClock(confInt["menuClock"]);
#endif

		if (confInt["backlightTimeout"] == 0) {
			if (PowerSaver::isRunning())
				delete PowerSaver::getInstance();
		} else {
			PowerSaver::getInstance()->setScreenTimeout( confInt["backlightTimeout"] );
		}

		if (lang == "English") lang = "";
		if (lang != tr.lang()) {
			tr.setLang(lang);
			confStr["lang"] = lang;
		}
		/*if (fileExists(CARD_ROOT) && !showRootFolder)
			unlink(CARD_ROOT);
		else if (!fileExists(CARD_ROOT) && showRootFolder)
			symlink("/", CARD_ROOT);*/
		//WARNING: the above might be dangerous with CARD_ROOT set to /
		writeConfig();
	}
}

void GMenu2X::skinMenu() {
	FileLister fl_sk(getHome() + "/skins", true, false);
	fl_sk.addExclude("..");
	fl_sk.browse();
	fl_sk.setPath(GMENU2X_SYSTEM_DIR "/skins", false);
	fl_sk.browse(false);

	string curSkin = confStr["skin"];

	SettingsDialog sd(this, input, ts, tr["Skin"]);
	sd.addSetting(new MenuSettingMultiString(this, ts, tr["Skin"], tr["Set the skin used by GMenu2X"], &confStr["skin"], &fl_sk.getDirectories()));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Top Bar Color"], tr["Color of the top bar"], &skinConfColors[COLOR_TOP_BAR_BG]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Bottom Bar Color"], tr["Color of the bottom bar"], &skinConfColors[COLOR_BOTTOM_BAR_BG]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Selection Color"], tr["Color of the selection and other interface details"], &skinConfColors[COLOR_SELECTION_BG]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Message Box Color"], tr["Background color of the message box"], &skinConfColors[COLOR_MESSAGE_BOX_BG]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Message Box Border Color"], tr["Border color of the message box"], &skinConfColors[COLOR_MESSAGE_BOX_BORDER]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Message Box Selection Color"], tr["Color of the selection of the message box"], &skinConfColors[COLOR_MESSAGE_BOX_SELECTION]));

	if (sd.exec() && sd.edited()) {
		if (curSkin != confStr["skin"]) {
			setSkin(confStr["skin"]);
			writeConfig();
		}
		writeSkinConfig();
		initBG();
	}
}

void GMenu2X::setSkin(const string &skin, bool setWallpaper) {
	confStr["skin"] = skin;

	//Clear previous skin settings
	skinConfStr.clear();
	skinConfInt.clear();

	DEBUG("GMenu2X: setting new skin %s.\n", skin.c_str());

	//clear collection and change the skin path
	sc.clear();
	sc.setSkin(skin);

	//reset colors to the default values
	skinConfColors[COLOR_TOP_BAR_BG] = (RGBAColor){255,255,255,130};
	skinConfColors[COLOR_BOTTOM_BAR_BG] = (RGBAColor){255,255,255,130};
	skinConfColors[COLOR_SELECTION_BG] = (RGBAColor){255,255,255,130};
	skinConfColors[COLOR_MESSAGE_BOX_BG] = (RGBAColor){255,255,255,255};
	skinConfColors[COLOR_MESSAGE_BOX_BORDER] = (RGBAColor){80,80,80,255};
	skinConfColors[COLOR_MESSAGE_BOX_SELECTION] = (RGBAColor){160,160,160,255};

	/* Load skin settings from user directory if present,
	 * or from the system directory. */
	string skinconfname = getHome() + "/skins/" + skin + "/skin.conf";
	if (!fileExists(skinconfname))
	  skinconfname = GMENU2X_SYSTEM_DIR "/skins/" + skin + "/skin.conf";

	if (fileExists(skinconfname)) {
		ifstream skinconf(skinconfname.c_str(), ios_base::in);
		if (skinconf.is_open()) {
			string line;
			while (getline(skinconf, line, '\n')) {
				line = trim(line);
				DEBUG("skinconf: '%s'\n", line.c_str());
				string::size_type pos = line.find("=");
				string name = trim(line.substr(0,pos));
				string value = trim(line.substr(pos+1,line.length()));

				if (value.length()>0) {
					if (value.length()>1 && value.at(0)=='"' && value.at(value.length()-1)=='"')
						skinConfStr[name] = value.substr(1,value.length()-2);
					else if (value.at(0) == '#')
						skinConfColors[stringToColor(name)] = strtorgba( value.substr(1,value.length()) );
					else
						skinConfInt[name] = atoi(value.c_str());
				}
			}
			skinconf.close();

			if (setWallpaper && !skinConfStr["wallpaper"].empty()) {
				string fp = sc.getSkinFilePath("wallpapers/" + skinConfStr["wallpaper"]);
				if (!fp.empty())
					confStr["wallpaper"] = fp;
				else
					WARNING("Unable to find wallpaper defined on skin %s\n", skin.c_str());
			}
		}
	}

	evalIntConf( &skinConfInt["topBarHeight"], 40, 32,120 );
	evalIntConf( &skinConfInt["linkHeight"], 40, 32,120 );
	evalIntConf( &skinConfInt["linkWidth"], 60, 32,120 );

	//recalculate some coordinates based on the new element sizes
	linkColumns = (resX-10)/skinConfInt["linkWidth"];
	linkRows = (resY-35-skinConfInt["topBarHeight"])/skinConfInt["linkHeight"];

	if (menu != NULL) menu->loadIcons();

	//Selection png
	useSelectionPng = sc.addSkinRes("imgs/selection.png", false) != NULL;

	//font
	initFont();
}

void GMenu2X::showManual() {
	menu->selLinkApp()->showManual();
}

void GMenu2X::contextMenu() {
	vector<MenuOption> voices;
	{
	MenuOption opt = {tr.translate("Add link in $1",menu->selSection().c_str(),NULL), BIND(&GMenu2X::addLink)};
	voices.push_back(opt);
	}

	{
		LinkApp* app = menu->selLinkApp();
		if (app && !app->getManual().empty()) {
			MenuOption opt = {tr.translate("Show manual of $1",menu->selLink()->getTitle().c_str(),NULL),
				BIND(&GMenu2X::showManual),
			};
			voices.push_back(opt);
		}
	}

	if (menu->selLinkApp()!=NULL && menu->selLinkApp()->isEditable()) {

/* FIXME(percuei): this permits to mask the "Edit link" entry
 * on the contextual menu in case CPUFREQ support is
 * not compiled in and the link corresponds to an OPK.
 * This is not a good idea as it'll break things if
 * a new config option is added to the contextual menu. */
#if defined(HAVE_LIBOPK) && !defined(ENABLE_CPUFREQ)
		if (!menu->selLinkApp()->isOpk() ||
					!menu->selLinkApp()->getSelectorDir().empty())
#endif
		{
		MenuOption opt = {tr.translate("Edit $1",menu->selLink()->getTitle().c_str(),NULL), BIND(&GMenu2X::editLink)};
		voices.push_back(opt);
		}
#ifdef HAVE_LIBOPK
		if (!menu->selLinkApp()->isOpk())
#endif
		{
		MenuOption opt = {tr.translate("Delete $1 link",menu->selLink()->getTitle().c_str(),NULL), BIND(&GMenu2X::deleteLink)};
		voices.push_back(opt);
		}
	}

	{
	MenuOption opt = {tr["Add section"], BIND(&GMenu2X::addSection)};
	voices.push_back(opt);
	}{
	MenuOption opt = {tr["Rename section"], BIND(&GMenu2X::renameSection)};
	voices.push_back(opt);
	}{
	MenuOption opt = {tr["Delete section"], BIND(&GMenu2X::deleteSection)};
	voices.push_back(opt);
	}{
	MenuOption opt = {tr["Scan for applications and games"], BIND(&GMenu2X::scanner)};
	voices.push_back(opt);
	}

	bool close = false;
	uint i, fadeAlpha=0;
	int sel = 0;

	int h = font->getHeight();
	SDL_Rect box;
	box.h = (h+2)*voices.size()+8;
	box.w = 0;
	for (i=0; i<voices.size(); i++) {
		int w = font->getTextWidth(voices[i].text);
		if (w>box.w) box.w = w;
	}
	box.w += 23;
	box.x = halfX - box.w/2;
	box.y = halfY - box.h/2;

	SDL_Rect selbox = {
		static_cast<Sint16>(box.x + 4),
		0,
		static_cast<Uint16>(box.w - 8),
		static_cast<Uint16>(h + 2)
	};
	long tickNow, tickStart = SDL_GetTicks();

	Surface bg(s);
	/*//Darken background
	bg.box(0, 0, resX, resY, 0,0,0,150);
	bg.box(box.x, box.y, box.w, box.h, skinConfColors["messageBoxBg"]);
	bg.rectangle( box.x+2, box.y+2, box.w-4, box.h-4, skinConfColors["messageBoxBorder"] );*/

    InputManager::ButtonEvent event;
	while (!close) {
		tickNow = SDL_GetTicks();

		selbox.y = box.y+4+(h+2)*sel;
		bg.blit(s,0,0);

		if (fadeAlpha<200) fadeAlpha = intTransition(0,200,tickStart,500,tickNow);
		s->box(0, 0, resX, resY, 0,0,0,fadeAlpha);
		s->box(box.x, box.y, box.w, box.h, skinConfColors[COLOR_MESSAGE_BOX_BG]);
		s->rectangle( box.x+2, box.y+2, box.w-4, box.h-4, skinConfColors[COLOR_MESSAGE_BOX_BORDER] );


		//draw selection rect
		s->box( selbox.x, selbox.y, selbox.w, selbox.h, skinConfColors[COLOR_MESSAGE_BOX_SELECTION] );
		for (i=0; i<voices.size(); i++)
			s->write( font, voices[i].text, box.x+12, box.y+5+(h+2)*i, Font::HAlignLeft, Font::VAlignTop );
		s->flip();

		//touchscreen
		if (ts.available()) {
			ts.poll();
			if (ts.released()) {
				if (!ts.inRect(box))
					close = true;
				else if (ts.getX() >= selbox.x
					  && ts.getX() <= selbox.x + selbox.w)
					for (i=0; i<voices.size(); i++) {
						selbox.y = box.y+4+(h+2)*i;
						if (ts.getY() >= selbox.y
						 && ts.getY() <= selbox.y + selbox.h) {
							voices[i].action();
							close = true;
							i = voices.size();
						}
					}
			} else if (ts.pressed() && ts.inRect(box)) {
				for (i=0; i<voices.size(); i++) {
					selbox.y = box.y+4+(h+2)*i;
					if (ts.getY() >= selbox.y
					 && ts.getY() <= selbox.y + selbox.h) {
						sel = i;
						i = voices.size();
					}
				}
			}
		}


        if (fadeAlpha < 200) {
            if (!input.pollEvent(&event) || event.state != InputManager::PRESSED) continue;
        } else {
            event.button = input.waitForPressedButton();
        }

        switch(event.button) {
            case InputManager::MENU:
                close = true;
                break;
            case InputManager::UP:
                sel = std::max(0, sel-1);
                break;
            case InputManager::DOWN:
                sel = std::min((int)voices.size()-1, sel+1);
                break;
            case InputManager::ACCEPT:
                voices[sel].action();
                close = true;
                break;
            default:
                break;
        }
	}
}

void GMenu2X::changeWallpaper() {
	WallpaperDialog wp(this, ts);
	if (wp.exec() && confStr["wallpaper"] != wp.wallpaper) {
		confStr["wallpaper"] = wp.wallpaper;
		initBG();
		writeConfig();
	}
}

void GMenu2X::addLink() {
	FileDialog fd(this, ts, tr["Select an application"], "dge,sh,bin,py,elf,");
	if (fd.exec()) {
		menu->addLink(fd.getPath(), fd.getFile());
		sync();
	}
}

void GMenu2X::editLink() {
	if (menu->selLinkApp()==NULL) return;

	vector<string> pathV;
	split(pathV,menu->selLinkApp()->getFile(),"/");
	string oldSection = "";
	if (pathV.size()>1)
		oldSection = pathV[pathV.size()-2];
	string newSection = oldSection;

	string linkTitle = menu->selLinkApp()->getTitle();
	string linkDescription = menu->selLinkApp()->getDescription();
	string linkIcon = menu->selLinkApp()->getIcon();
	string linkManual = menu->selLinkApp()->getManual();
	string linkSelFilter = menu->selLinkApp()->getSelectorFilter();
	string linkSelDir = menu->selLinkApp()->getSelectorDir();
	bool linkSelBrowser = menu->selLinkApp()->getSelectorBrowser();
	string linkSelScreens = menu->selLinkApp()->getSelectorScreens();
	string linkSelAliases = menu->selLinkApp()->getAliasFile();
	int linkClock = menu->selLinkApp()->clock();

	string diagTitle = tr.translate("Edit link: $1",linkTitle.c_str(),NULL);
	string diagIcon = menu->selLinkApp()->getIconPath();

	SettingsDialog sd(this, input, ts, diagTitle, diagIcon);
#ifdef HAVE_LIBOPK
	if (!menu->selLinkApp()->isOpk()) {
#endif
	sd.addSetting(new MenuSettingString(this, ts, tr["Title"], tr["Link title"], &linkTitle, diagTitle, diagIcon));
	sd.addSetting(new MenuSettingString(this, ts, tr["Description"], tr["Link description"], &linkDescription, diagTitle, diagIcon));
	sd.addSetting(new MenuSettingMultiString(this, ts, tr["Section"], tr["The section this link belongs to"], &newSection, &menu->getSections()));
	sd.addSetting(new MenuSettingImage(this, ts, tr["Icon"],
					tr.translate("Select an icon for the link: $1",
						linkTitle.c_str(), NULL), &linkIcon, "png"));
	sd.addSetting(new MenuSettingFile(this, ts, tr["Manual"],
					tr["Select a graphic/textual manual or a readme"],
					&linkManual, "man.png,txt"));
#ifdef HAVE_LIBOPK
	}
	if (!menu->selLinkApp()->isOpk() ||
				!menu->selLinkApp()->getSelectorDir().empty()) {
#endif
	sd.addSetting(new MenuSettingDir(this, ts, tr["Selector Directory"], tr["Directory to scan for the selector"], &linkSelDir));
	sd.addSetting(new MenuSettingBool(this, ts, tr["Selector Browser"], tr["Allow the selector to change directory"], &linkSelBrowser));
#ifdef HAVE_LIBOPK
	}
#endif
#ifdef ENABLE_CPUFREQ
	sd.addSetting(new MenuSettingInt(this, ts, tr["Clock frequency"], tr["Cpu clock frequency to set when launching this link"], &linkClock, cpuFreqMin, confInt["maxClock"], cpuFreqMultiple));
#endif
#ifdef HAVE_LIBOPK
	if (!menu->selLinkApp()->isOpk()) {
#endif
	sd.addSetting(new MenuSettingString(this, ts, tr["Selector Filter"], tr["Selector filter (Separate values with a comma)"], &linkSelFilter, diagTitle, diagIcon));
	sd.addSetting(new MenuSettingDir(this, ts, tr["Selector Screenshots"], tr["Directory of the screenshots for the selector"], &linkSelScreens));
	sd.addSetting(new MenuSettingFile(this, ts, tr["Selector Aliases"], tr["File containing a list of aliases for the selector"], &linkSelAliases));
	sd.addSetting(new MenuSettingBool(this, ts, tr["Don't Leave"], tr["Don't quit GMenu2X when launching this link"], &menu->selLinkApp()->runsInBackgroundRef()));
#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0)
	sd.addSetting(new MenuSettingBool(this, ts, tr["Display Console"], tr["Must be enabled for console-based applications"], &menu->selLinkApp()->consoleApp));
#endif
#ifdef HAVE_LIBOPK
	}
#endif

	if (sd.exec() && sd.edited()) {
		menu->selLinkApp()->setTitle(linkTitle);
		menu->selLinkApp()->setDescription(linkDescription);
		menu->selLinkApp()->setIcon(linkIcon);
		menu->selLinkApp()->setManual(linkManual);
		menu->selLinkApp()->setSelectorFilter(linkSelFilter);
		menu->selLinkApp()->setSelectorDir(linkSelDir);
		menu->selLinkApp()->setSelectorBrowser(linkSelBrowser);
		menu->selLinkApp()->setSelectorScreens(linkSelScreens);
		menu->selLinkApp()->setAliasFile(linkSelAliases);
		menu->selLinkApp()->setClock(linkClock);

		INFO("New Section: '%s'\n", newSection.c_str());

		//if section changed move file and update link->file
		if (oldSection!=newSection) {
			vector<string>::const_iterator newSectionIndex = find(menu->getSections().begin(),menu->getSections().end(),newSection);
			if (newSectionIndex==menu->getSections().end()) return;
			string newFileName = "sections/"+newSection+"/"+linkTitle;
			uint x=2;
			while (fileExists(newFileName)) {
				string id = "";
				stringstream ss; ss << x; ss >> id;
				newFileName = "sections/"+newSection+"/"+linkTitle+id;
				x++;
			}
			rename(menu->selLinkApp()->getFile().c_str(),newFileName.c_str());
			menu->selLinkApp()->renameFile(newFileName);

			INFO("New section index: %i.\n", newSectionIndex - menu->getSections().begin());

			menu->linkChangeSection(menu->selLinkIndex(), menu->selSectionIndex(), newSectionIndex - menu->getSections().begin());
		}
		menu->selLinkApp()->save();
		sync();
	}
}

void GMenu2X::deleteLink() {
	if (menu->selLinkApp()!=NULL) {
		MessageBox mb(this, tr.translate("Deleting $1",menu->selLink()->getTitle().c_str(),NULL)+"\n"+tr["Are you sure?"], menu->selLink()->getIconPath());
		mb.setButton(InputManager::ACCEPT, tr["Yes"]);
		mb.setButton(InputManager::CANCEL, tr["No"]);
		if (mb.exec() == InputManager::ACCEPT) {
			menu->deleteSelectedLink();
			sync();
		}
	}
}

void GMenu2X::addSection() {
	InputDialog id(this, input, ts, tr["Insert a name for the new section"]);
	if (id.exec()) {
		//only if a section with the same name does not exist
		if (find(menu->getSections().begin(), menu->getSections().end(), id.getInput())
				== menu->getSections().end()) {
			//section directory doesn't exists
			if (menu->addSection(id.getInput())) {
				menu->setSectionIndex( menu->getSections().size()-1 ); //switch to the new section
				sync();
			}
		}
	}
}

void GMenu2X::renameSection() {
	InputDialog id(this, input, ts, tr["Insert a new name for this section"],menu->selSection());
	if (id.exec()) {
		//only if a section with the same name does not exist & !samename
		if (menu->selSection() != id.getInput()
		 && find(menu->getSections().begin(),menu->getSections().end(), id.getInput())
				== menu->getSections().end()) {
			//section directory doesn't exists
			string newsectiondir = getHome() + "/sections/" + id.getInput();
			string sectiondir = getHome() + "/sections/" + menu->selSection();

			if (!rename(sectiondir.c_str(), newsectiondir.c_str())) {
				string oldpng = menu->selSection() + ".png";
				string newpng = id.getInput() + ".png";
				string oldicon = sc.getSkinFilePath(oldpng);
				string newicon = sc.getSkinFilePath(newpng);

				if (!oldicon.empty() && newicon.empty()) {
					newicon = oldicon;
					newicon.replace(newicon.find(oldpng), oldpng.length(), newpng);

					if (!fileExists(newicon)) {
						rename(oldicon.c_str(), newicon.c_str());
						sc.move("skin:"+oldpng, "skin:"+newpng);
					}
				}
				menu->renameSection(menu->selSectionIndex(), id.getInput());
				sync();
			}
		}
	}
}

void GMenu2X::deleteSection() {
	MessageBox mb(this,tr["You will lose all the links in this section."]+"\n"+tr["Are you sure?"]);
	mb.setButton(InputManager::ACCEPT, tr["Yes"]);
	mb.setButton(InputManager::CANCEL, tr["No"]);
	if (mb.exec() == InputManager::ACCEPT) {

		if (rmtree(getHome() + "/sections/" + menu->selSection())) {
			menu->deleteSelectedSection();
			sync();
		}
	}
}

void GMenu2X::scanner() {
	Surface scanbg(bg);
	drawButton(&scanbg, "cancel", tr["Exit"],
	drawButton(&scanbg, "accept", "", 5)-10);
	scanbg.write(font,tr["Link Scanner"],halfX,7,Font::HAlignCenter,Font::VAlignMiddle);
	scanbg.convertToDisplayFormat();

	uint lineY = 42;

#ifdef PLATFORM_PANDORA
	//char *configpath = pnd_conf_query_searchpath();
#else
#ifdef ENABLE_CPUFREQ
	setSafeMaxClock();
#endif

	scanbg.write(font,tr["Scanning filesystem..."],5,lineY);
	scanbg.blit(s,0,0);
	s->flip();
	lineY += 26;

	vector<string> files;
	scanPath(CARD_ROOT, &files);

	stringstream ss;
	ss << files.size();
	string str = "";
	ss >> str;
	scanbg.write(font,tr.translate("$1 files found.",str.c_str(),NULL),5,lineY);
	lineY += 26;
	scanbg.write(font,tr["Creating links..."],5,lineY);
	scanbg.blit(s,0,0);
	s->flip();
	lineY += 26;

	string path, file;
	string::size_type pos;
	uint linkCount = 0;

	for (uint i = 0; i<files.size(); i++) {
		pos = files[i].rfind("/");
		if (pos!=string::npos && pos>0) {
			path = files[i].substr(0, pos+1);
			file = files[i].substr(pos+1, files[i].length());
			if (menu->addLink(path,file,"found "+file.substr(file.length()-3,3)))
				linkCount++;
		}
	}

	ss.clear();
	ss << linkCount;
	ss >> str;
	scanbg.write(font,tr.translate("$1 links created.",str.c_str(),NULL),5,lineY);
	scanbg.blit(s,0,0);
	s->flip();
	lineY += 26;

#ifdef ENABLE_CPUFREQ
	setMenuClock();
#endif
	sync();
#endif

    InputManager::Button button;
    do {
        button = input.waitForPressedButton();
    } while ((button != InputManager::SETTINGS)
                && (button != InputManager::ACCEPT)
                && (button != InputManager::CANCEL));
}

void GMenu2X::scanPath(string path, vector<string> *files) {
	DIR *dirp;
	struct stat st;
	struct dirent *dptr;
	string filepath, ext;

	if (path[path.length()-1]!='/') path += "/";
	if ((dirp = opendir(path.c_str())) == NULL) return;

	while ((dptr = readdir(dirp))) {
		if (dptr->d_name[0]=='.')
			continue;
		filepath = path+dptr->d_name;
		int statRet = stat(filepath.c_str(), &st);
		if (S_ISDIR(st.st_mode))
			scanPath(filepath, files);
		if (statRet != -1) {
			ext = filepath.substr(filepath.length()-4,4);
#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0) || defined(PLATFORM_NANONOTE)
			if (ext==".dge")
#else
			if (ext==".pxml")
#endif
				files->push_back(filepath);
		}
	}

	closedir(dirp);
}

typedef struct {
	unsigned short batt;
	unsigned short remocon;
} MMSP2ADC;

unsigned short GMenu2X::getBatteryLevel() {
	FILE *batteryHandle = NULL,
		 *usbHandle = NULL;

#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0) || defined(PLATFORM_NANONOTE)
	usbHandle = fopen("/sys/class/power_supply/usb/online", "r");
#endif
	if (usbHandle) {
		int usbval = 0;
		fscanf(usbHandle, "%d", &usbval);
		fclose(usbHandle);
		if (usbval == 1)
			return 6;
	}

#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0) || defined(PLATFORM_NANONOTE)
	batteryHandle = fopen("/sys/class/power_supply/battery/capacity", "r");
#endif
	if (batteryHandle) {
		int battval = 0;
		fscanf(batteryHandle, "%d", &battval);
		fclose(batteryHandle);

		if (battval>90) return 5;
		if (battval>70) return 4;
		if (battval>50) return 3;
		if (battval>30) return 2;
		if (battval>10) return 1;
	}

	return 0;
}

void GMenu2X::setInputSpeed() {
	SDL_EnableKeyRepeat(1,150);
}

#ifdef ENABLE_CPUFREQ
void GMenu2X::setClock(unsigned mhz) {
	mhz = constrain(mhz, cpuFreqMin, confInt["maxClock"]);
#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0) || defined(PLATFORM_NANONOTE)
	jz_cpuspeed(mhz);
#endif
}
#endif

string GMenu2X::getDiskFree(const char *path) {
	stringstream ss;
	string df = "";
	struct statvfs b;

	int ret = statvfs(path, &b);
	if (ret==0) {
		// Make sure that the multiplication happens in 64 bits.
		unsigned long long free =
			((unsigned long long)b.f_bfree * b.f_bsize) / 1048576;
		unsigned long long total =
			((unsigned long long)b.f_blocks * b.f_frsize) / 1048576;
		ss << free << "/" << total << "MB";
		ss >> df;
	} else WARNING("statvfs failed with error '%s'.\n", strerror(errno));
	return df;
}

int GMenu2X::drawButton(Button *btn, int x, int y) {
	if (y<0) y = resY+y;
	btn->setPosition(x, y-7);
	btn->paint();
	return x+btn->getRect().w+6;
}

int GMenu2X::drawButton(Surface *s, const string &btn, const string &text, int x, int y) {
	if (y<0) y = resY+y;
	SDL_Rect re = { static_cast<Sint16>(x), static_cast<Sint16>(y - 7), 0, 16 };
	if (sc.skinRes("imgs/buttons/"+btn+".png") != NULL) {
		sc["imgs/buttons/"+btn+".png"]->blit(s, x, y-7);
		re.w = sc["imgs/buttons/"+btn+".png"]->width() + 3;
		s->write(font, text, x+re.w, y, Font::HAlignLeft, Font::VAlignMiddle);
		re.w += font->getTextWidth(text);
	}
	return x+re.w+6;
}

int GMenu2X::drawButtonRight(Surface *s, const string &btn, const string &text, int x, int y) {
	if (y<0) y = resY+y;
	if (sc.skinRes("imgs/buttons/"+btn+".png") != NULL) {
		x -= 16;
		sc["imgs/buttons/"+btn+".png"]->blit(s, x, y-7);
		x -= 3;
		s->write(font, text, x, y, Font::HAlignRight, Font::VAlignMiddle);
		return x-6-font->getTextWidth(text);
	}
	return x-6;
}

void GMenu2X::drawScrollBar(uint pagesize, uint totalsize, uint pagepos, uint top, uint height) {
	if (totalsize<=pagesize) return;

	s->rectangle(resX-8, top, 7, height, skinConfColors[COLOR_SELECTION_BG]);

	//internal bar total height = height-2
	//bar size
	uint bs = (height-2) * pagesize / totalsize;
	//bar y position
	uint by = (height-2) * pagepos / totalsize;
	by = top+2+by;
	if (by+bs>top+height-2) by = top+height-2-bs;


	s->box(resX-6, by, 3, bs, skinConfColors[COLOR_SELECTION_BG]);
}

void GMenu2X::drawTopBar(Surface *s) {
	if (s==NULL) s = this->s;

	Surface *bar = sc.skinRes("imgs/topbar.png", false);
	if (bar != NULL)
		bar->blit(s, 0, 0);
	else
		s->box(0, 0, resX, skinConfInt["topBarHeight"],
		skinConfColors[COLOR_TOP_BAR_BG]);
}

void GMenu2X::drawBottomBar(Surface *s) {
	if (s==NULL) s = this->s;

	Surface *bar = sc.skinRes("imgs/bottombar.png", false);
	if (bar != NULL)
		bar->blit(s, 0, resY-bar->height());
	else
		s->box(0, resY-20, resX, 20, skinConfColors[COLOR_BOTTOM_BAR_BG]);
}
