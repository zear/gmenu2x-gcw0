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

#include "background.h"
#include "cpu.h"
#include "debug.h"
#include "filedialog.h"
#include "filelister.h"
#include "font.h"
#include "gmenu2x.h"
#include "helppopup.h"
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
	: appToLaunch(nullptr)
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

	if( SDL_Init(SDL_INIT_TIMER) < 0) {
		ERROR("Could not initialize SDL: %s\n", SDL_GetError());
		quit();
	}

	bg = NULL;
	font = NULL;
	setSkin(confStr["skin"], !fileExists(confStr["wallpaper"]));
	layers.insert(layers.begin(), make_shared<Background>(*this));

	/* We enable video at a later stage, so that the menu elements are
	 * loaded before SDL inits the video; this is made so that we won't show
	 * a black screen for a couple of seconds. */
	if( SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		ERROR("Could not initialize SDL: %s\n", SDL_GetError());
		quit();
	}

	s = Surface::openOutputSurface(resX, resY, confInt["videoBpp"]);

	if (!fileExists(confStr["wallpaper"])) {
		DEBUG("No wallpaper defined; we will take the default one.\n");
		confStr["wallpaper"] = DEFAULT_WALLPAPER_PATH;
	}

	initBG();

	/* the menu may take a while to load, so we show the background here */
	for (auto layer : layers)
		layer->paint(*s);
	s->flip();

	initMenu();

#ifdef ENABLE_INOTIFY
	monitor = new MediaMonitor(CARD_ROOT);
#endif

	/* If a user-specified input.conf file exists, we load it;
	 * otherwise, we load the default one. */
	string input_file = getHome() + "/input.conf";
	if (fileExists(input_file.c_str())) {
		DEBUG("Loading user-specific input.conf file: %s.\n", input_file.c_str());
	} else {
		input_file = GMENU2X_SYSTEM_DIR "/input.conf";
		DEBUG("Loading system input.conf file: %s.\n", input_file.c_str());
	}

	input.init(input_file, menu.get());

	if (confInt["backlightTimeout"] > 0)
        PowerSaver::getInstance()->setScreenTimeout( confInt["backlightTimeout"] );

	SDL_EnableKeyRepeat(INPUT_KEY_REPEAT_DELAY, INPUT_KEY_REPEAT_RATE);
#ifdef ENABLE_CPUFREQ
	setClock(confInt["menuClock"]);
#endif
}

GMenu2X::~GMenu2X() {
	if (PowerSaver::isRunning())
		delete PowerSaver::getInstance();
	quit();

	delete font;
#ifdef ENABLE_INOTIFY
	delete monitor;
#endif
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

	string df = getDiskFree(getHome().c_str());
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
	if (font) {
		delete font;
		font = NULL;
	}

	string path = skinConfStr["font"];
	if (!path.empty()) {
		unsigned int size = skinConfInt["fontsize"];
		if (!size)
			size = 12;
		if (path.substr(0,5)=="skin:")
			path = sc.getSkinFilePath(path.substr(5, path.length()));
		font = new Font(path, size);
	} else {
		font = Font::defaultFont();
	}

	if (!font) {
		ERROR("Cannot function without font; aborting...\n");
		quit();
		exit(-1);
	}
}

void GMenu2X::initMenu() {
	//Menu structure handler
	menu.reset(new Menu(this, ts));
	for (uint i=0; i<menu->getSections().size(); i++) {
		//Add virtual links in the applications section
		if (menu->getSections()[i]=="applications") {
			menu->addActionLink(i,"Explorer", BIND(&GMenu2X::explorer),tr["Launch an application"],"skin:icons/explorer.png");
		}

		//Add virtual links in the setting section
		else if (menu->getSections()[i]=="settings") {
			menu->addActionLink(i,"GMenu2X",BIND(&GMenu2X::showSettings),tr["Configure GMenu2X's options"],"skin:icons/configure.png");
			menu->addActionLink(i,tr["Skin"],BIND(&GMenu2X::skinMenu),tr["Configure skin"],"skin:icons/skin.png");
			menu->addActionLink(i,tr["Wallpaper"],BIND(&GMenu2X::changeWallpaper),tr["Change GMenu2X wallpaper"],"skin:icons/wallpaper.png");
			if (fileExists(LOG_FILE))
				menu->addActionLink(i,tr["Log Viewer"],BIND(&GMenu2X::viewLog),tr["Displays last launched program's output"],"skin:icons/ebook.png");
			menu->addActionLink(i,tr["About"],BIND(&GMenu2X::about),tr["Info about GMenu2X"],"skin:icons/about.png");
		}
	}

	menu->skinUpdated();
	menu->orderLinks();

	menu->setSectionIndex(confInt["section"]);
	menu->setLinkIndex(confInt["link"]);

	layers.push_back(menu);
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
	string logfile = LOG_FILE;
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

	evalIntConf( confInt, "outputLogs", 0, 0,1 );
#ifdef ENABLE_CPUFREQ
	evalIntConf( confInt, "maxClock",
				 cpuFreqSafeMax, cpuFreqMin, cpuFreqMax );
	evalIntConf( confInt, "menuClock",
				 cpuFreqMenuDefault, cpuFreqMin, cpuFreqSafeMax );
#endif
	evalIntConf( confInt, "backlightTimeout", 15, 0,120 );
	evalIntConf( confInt, "videoBpp", 32, 16, 32 );

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
		if (!selectordir.empty())
			inf << "selectordir=" << selectordir << endl;
		inf.close();
	}
}

void GMenu2X::main() {
	if (!fileExists(CARD_ROOT))
		CARD_ROOT = "";

	appToLaunch = nullptr;

	// Recover last session
	readTmp();
	if (lastSelectorElement > -1 && menu->selLinkApp() &&
				(!menu->selLinkApp()->getSelectorDir().empty()
				 || !lastSelectorDir.empty()))
		menu->selLinkApp()->selector(lastSelectorElement, lastSelectorDir);

	while (true) {
		// Remove dismissed layers from the stack.
		for (auto it = layers.begin(); it != layers.end(); ) {
			if ((*it)->getStatus() == Layer::Status::DISMISSED) {
				it = layers.erase(it);
			} else {
				++it;
			}
		}

		// Run animations.
		bool animating = false;
		for (auto layer : layers) {
			animating |= layer->runAnimations();
		}

		// Paint layers.
		for (auto layer : layers) {
			layer->paint(*s);
		}
		if (appToLaunch) {
			break;
		}
		s->flip();

		// Handle touchscreen events.
		if (ts.available()) {
			ts.poll();
			for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
				if ((*it)->handleTouchscreen(ts)) {
					break;
				}
			}
		}

		// Handle other input events.
		InputManager::Button button;
		bool gotEvent;
		const bool wait = !animating;
		do {
			gotEvent = input.getButton(&button, wait);
		} while (wait && !gotEvent);
		if (gotEvent) {
			for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
				if ((*it)->handleButtonPress(button)) {
					break;
				}
			}
		}
	}

	if (appToLaunch) {
		appToLaunch->drawRun();
		appToLaunch->launch(fileToLaunch);
	}
}

void GMenu2X::explorer() {
	FileDialog fd(this, ts, tr["Select an application"], "sh,bin,py,elf,");
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

void GMenu2X::queueLaunch(LinkApp *app, const std::string &file) {
	appToLaunch = app;
	fileToLaunch = file;
}

void GMenu2X::showHelpPopup() {
	layers.push_back(make_shared<HelpPopup>(*this));
}

void GMenu2X::showSettings() {
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
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Top Bar"], tr["Color of the top bar"], &skinConfColors[COLOR_TOP_BAR_BG]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Bottom Bar"], tr["Color of the bottom bar"], &skinConfColors[COLOR_BOTTOM_BAR_BG]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Selection"], tr["Color of the selection and other interface details"], &skinConfColors[COLOR_SELECTION_BG]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Message Box"], tr["Background color of the message box"], &skinConfColors[COLOR_MESSAGE_BOX_BG]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Message Box Border"], tr["Border color of the message box"], &skinConfColors[COLOR_MESSAGE_BOX_BORDER]));
	sd.addSetting(new MenuSettingRGBA(this, ts, tr["Message Box Selection"], tr["Color of the selection of the message box"], &skinConfColors[COLOR_MESSAGE_BOX_SELECTION]));

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

	evalIntConf(skinConfInt, "topBarHeight", 50, 32, 120);
	evalIntConf(skinConfInt, "bottomBarHeight", 20, 20, 120);
	evalIntConf(skinConfInt, "linkHeight", 50, 32, 120);
	evalIntConf(skinConfInt, "linkWidth", 80, 32, 120);

	if (menu != NULL) menu->skinUpdated();

	//Selection png
	useSelectionPng = sc.addSkinRes("imgs/selection.png", false) != NULL;

	//font
	initFont();
}

void GMenu2X::showManual() {
	menu->selLinkApp()->showManual();
}

void GMenu2X::showContextMenu() {
	layers.push_back(make_shared<ContextMenu>(*this, *menu));
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
	FileDialog fd(this, ts, tr["Select an application"], "sh,bin,py,elf,");
	if (fd.exec())
		menu->addLink(fd.getPath(), fd.getFile());
}

void GMenu2X::editLink() {
	LinkApp *linkApp = menu->selLinkApp();
	if (!linkApp) return;

	vector<string> pathV;
	split(pathV,linkApp->getFile(),"/");
	string oldSection = "";
	if (pathV.size()>1)
		oldSection = pathV[pathV.size()-2];
	string newSection = oldSection;

	string linkTitle = linkApp->getTitle();
	string linkDescription = linkApp->getDescription();
	string linkIcon = linkApp->getIcon();
	string linkManual = linkApp->getManual();
	string linkSelFilter = linkApp->getSelectorFilter();
	string linkSelDir = linkApp->getSelectorDir();
	bool linkSelBrowser = linkApp->getSelectorBrowser();
	string linkSelAliases = linkApp->getAliasFile();
	int linkClock = linkApp->clock();

	string diagTitle = tr.translate("Edit link: $1",linkTitle.c_str(),NULL);
	string diagIcon = linkApp->getIconPath();

	SettingsDialog sd(this, input, ts, diagTitle, diagIcon);
	if (!linkApp->isOpk()) {
		sd.addSetting(new MenuSettingString(this, ts, tr["Title"], tr["Link title"], &linkTitle, diagTitle, diagIcon));
		sd.addSetting(new MenuSettingString(this, ts, tr["Description"], tr["Link description"], &linkDescription, diagTitle, diagIcon));
		sd.addSetting(new MenuSettingMultiString(this, ts, tr["Section"], tr["The section this link belongs to"], &newSection, &menu->getSections()));
		sd.addSetting(new MenuSettingImage(this, ts, tr["Icon"],
						tr.translate("Select an icon for the link: $1",
							linkTitle.c_str(), NULL), &linkIcon, "png"));
		sd.addSetting(new MenuSettingFile(this, ts, tr["Manual"],
						tr["Select a graphic/textual manual or a readme"],
						&linkManual, "man.png,txt"));
	}
	if (!linkApp->isOpk() || !linkApp->getSelectorDir().empty()) {
		sd.addSetting(new MenuSettingDir(this, ts, tr["Selector Directory"], tr["Directory to scan for the selector"], &linkSelDir));
		sd.addSetting(new MenuSettingBool(this, ts, tr["Selector Browser"], tr["Allow the selector to change directory"], &linkSelBrowser));
	}
#ifdef ENABLE_CPUFREQ
	sd.addSetting(new MenuSettingInt(this, ts, tr["Clock frequency"], tr["Cpu clock frequency to set when launching this link"], &linkClock, cpuFreqMin, confInt["maxClock"], cpuFreqMultiple));
#endif
	if (!linkApp->isOpk()) {
		sd.addSetting(new MenuSettingString(this, ts, tr["Selector Filter"], tr["Selector filter (Separate values with a comma)"], &linkSelFilter, diagTitle, diagIcon));
		sd.addSetting(new MenuSettingFile(this, ts, tr["Selector Aliases"], tr["File containing a list of aliases for the selector"], &linkSelAliases));
#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0)
		sd.addSetting(new MenuSettingBool(this, ts, tr["Display Console"], tr["Must be enabled for console-based applications"], &linkApp->consoleApp));
#endif
	}

	if (sd.exec() && sd.edited()) {
		linkApp->setTitle(linkTitle);
		linkApp->setDescription(linkDescription);
		linkApp->setIcon(linkIcon);
		linkApp->setManual(linkManual);
		linkApp->setSelectorFilter(linkSelFilter);
		linkApp->setSelectorDir(linkSelDir);
		linkApp->setAliasFile(linkSelAliases);
		linkApp->setClock(linkClock);

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
			rename(linkApp->getFile().c_str(),newFileName.c_str());
			linkApp->renameFile(newFileName);

			INFO("New section index: %i.\n", newSectionIndex - menu->getSections().begin());

			menu->linkChangeSection(menu->selLinkIndex(), menu->selSectionIndex(), newSectionIndex - menu->getSections().begin());
		}
		linkApp->save();
	}
}

void GMenu2X::deleteLink() {
	if (menu->selLinkApp()!=NULL) {
		MessageBox mb(this, tr.translate("Deleting $1",menu->selLink()->getTitle().c_str(),NULL)+"\n"+tr["Are you sure?"], menu->selLink()->getIconPath());
		mb.setButton(InputManager::ACCEPT, tr["Yes"]);
		mb.setButton(InputManager::CANCEL, tr["No"]);
		if (mb.exec() == InputManager::ACCEPT)
			menu->deleteSelectedLink();
	}
}

void GMenu2X::addSection() {
	InputDialog id(this, input, ts, tr["Insert a name for the new section"]);
	if (id.exec()) {
		//only if a section with the same name does not exist
		if (find(menu->getSections().begin(), menu->getSections().end(), id.getInput())
				== menu->getSections().end()) {
			//section directory doesn't exists
			if (menu->addSection(id.getInput()))
				menu->setSectionIndex( menu->getSections().size()-1 ); //switch to the new section
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
			}
		}
	}
}

void GMenu2X::deleteSection() {
	MessageBox mb(this,tr["You will lose all the links in this section."]+"\n"+tr["Are you sure?"]);
	mb.setButton(InputManager::ACCEPT, tr["Yes"]);
	mb.setButton(InputManager::CANCEL, tr["No"]);
	if (mb.exec() == InputManager::ACCEPT) {

		if (rmtree(getHome() + "/sections/" + menu->selSection()))
			menu->deleteSelectedSection();
	}
}

typedef struct {
	unsigned short batt;
	unsigned short remocon;
} MMSP2ADC;

#ifdef ENABLE_CPUFREQ
void GMenu2X::setClock(unsigned mhz) {
	mhz = constrain(mhz, cpuFreqMin, confInt["maxClock"]);
#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0) || defined(PLATFORM_NANONOTE)
	jz_cpuspeed(mhz);
#endif
}
#endif

string GMenu2X::getDiskFree(const char *path) {
	string df = "";
	struct statvfs b;

	int ret = statvfs(path, &b);
	if (ret == 0) {
		// Make sure that the multiplication happens in 64 bits.
		unsigned long freeMiB =
				((unsigned long long)b.f_bfree * b.f_bsize) / (1024 * 1024);
		unsigned long totalMiB =
				((unsigned long long)b.f_blocks * b.f_frsize) / (1024 * 1024);
		stringstream ss;
		if (totalMiB >= 10000) {
			ss << (freeMiB / 1024) << "." << ((freeMiB % 1024) * 10) / 1024 << "/"
			   << (totalMiB / 1024) << "." << ((totalMiB % 1024) * 10) / 1024 << "GiB";
		} else {
			ss << freeMiB << "/" << totalMiB << "MiB";
		}
		ss >> df;
	} else WARNING("statvfs failed with error '%s'.\n", strerror(errno));
	return df;
}

int GMenu2X::drawButton(Surface *s, IconButton *btn, int x, int y) {
	if (y<0) y = resY+y;
	btn->setPosition(x, y-7);
	btn->paint(s);
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

void GMenu2X::drawScrollBar(uint pageSize, uint totalSize, uint pagePos) {
	if (totalSize <= pageSize) {
		// Everything fits on one screen, no scroll bar needed.
		return;
	}

	unsigned int top, height;
	tie(top, height) = getContentArea();
	top += 1;
	height -= 2;

	s->rectangle(resX - 8, top, 7, height, skinConfColors[COLOR_SELECTION_BG]);
	top += 2;
	height -= 4;

	const uint barSize = height * pageSize / totalSize;
	const uint barPos = (height - barSize) * pagePos / (totalSize - pageSize);

	s->box(resX - 6, top + barPos, 3, barSize,
			skinConfColors[COLOR_SELECTION_BG]);
}

void GMenu2X::drawTopBar(Surface *s) {
	Surface *bar = sc.skinRes("imgs/topbar.png", false);
	if (bar) {
		bar->blit(s, 0, 0);
	} else {
		const int h = skinConfInt["topBarHeight"];
		s->box(0, 0, resX, h, skinConfColors[COLOR_TOP_BAR_BG]);
	}
}

void GMenu2X::drawBottomBar(Surface *s) {
	Surface *bar = sc.skinRes("imgs/bottombar.png", false);
	if (bar) {
		bar->blit(s, 0, resY-bar->height());
	} else {
		const int h = skinConfInt["bottomBarHeight"];
		s->box(0, resY - h, resX, h, skinConfColors[COLOR_BOTTOM_BAR_BG]);
	}
}
