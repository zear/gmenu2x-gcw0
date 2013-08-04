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

#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <algorithm>
#include <math.h>
#include <fstream>
#include <unistd.h>

#ifdef HAVE_LIBOPK
#include <opk.h>
#endif

#include "gmenu2x.h"
#include "linkapp.h"
#include "menu.h"
#include "monitor.h"
#include "filelister.h"
#include "utilities.h"
#include "debug.h"

using namespace std;

Menu::Menu(GMenu2X *gmenu2x, Touchscreen &ts)
	: gmenu2x(gmenu2x)
	, ts(ts)
{
	iFirstDispSection = 0;

	readSections(GMENU2X_SYSTEM_DIR "/sections");
	readSections(GMenu2X::getHome() + "/sections");

	sort(sections.begin(),sections.end(),case_less());
	setSectionIndex(0);
	readLinks();

#ifdef HAVE_LIBOPK
	{
		struct dirent *dptr;
		DIR *dirp = opendir(CARD_ROOT);
		if (dirp) {
			while ((dptr = readdir(dirp))) {
				if (dptr->d_type != DT_DIR)
					continue;

				if (!strcmp(dptr->d_name, ".") || !strcmp(dptr->d_name, ".."))
					continue;

				openPackagesFromDir((string) CARD_ROOT + "/" +
							dptr->d_name + "/apps");
			}
			closedir(dirp);
		}
	}
#endif

	orderLinks();
}

Menu::~Menu() {
	freeLinks();

	for (vector<Monitor *>::iterator it = monitors.begin();
				it < monitors.end(); it++)
		delete *it;
}

void Menu::readSections(std::string parentDir)
{
	DIR *dirp;
	struct dirent *dptr;

	dirp = opendir(parentDir.c_str());
	if (!dirp) return;

	while ((dptr = readdir(dirp))) {
		if (dptr->d_name[0] == '.' || dptr->d_type != DT_DIR)
			continue;

		string filepath = parentDir + "/" + dptr->d_name;

		if (find(sections.begin(), sections.end(), (string)dptr->d_name) == sections.end()) {
			sections.push_back((string)dptr->d_name);
			vector<Link*> ll;
			links.push_back(ll);
		}
	}

	closedir(dirp);
}

void Menu::loadIcons() {
	//reload section icons
	vector<string>::size_type i = 0;
	for (string sectionName : sections) {
		string sectionIcon = "sections/" + sectionName + ".png";
		if (!gmenu2x->sc.getSkinFilePath(sectionIcon).empty())
			gmenu2x->sc.add("skin:" + sectionIcon);

		for (Link *&link : links[i]) {
			link->loadIcon();
		}

		i++;
	}
}

void Menu::paint(Surface &s) {
	const uint width = s.width(), height = s.height();
	const uint linkColumns = gmenu2x->linkColumns, linkRows = gmenu2x->linkRows;
	Font &font = *gmenu2x->font;
	SurfaceCollection sc = gmenu2x->sc;

	ConfIntHash &skinConfInt = gmenu2x->skinConfInt;
	const int topBarHeight = skinConfInt["topBarHeight"];
	const int bottomBarHeight = 21;
	const int linkWidth = skinConfInt["linkWidth"];
	const int linkHeight = skinConfInt["linkHeight"];
	RGBAColor &selectionBgColor = gmenu2x->skinConfColors[COLOR_SELECTION_BG];

	//Sections
	const uint sectionLinkPadding = (topBarHeight - 32 - font.getHeight()) / 3;
	const uint sectionsCoordX =
			(width - constrain((uint)sections.size(), 0 , linkColumns) * linkWidth) / 2;
	if (iFirstDispSection > 0) {
		sc.skinRes("imgs/l_enabled.png")->blit(&s, 0, 0);
	} else {
		sc.skinRes("imgs/l_disabled.png")->blit(&s, 0, 0);
	}
	if (iFirstDispSection + linkColumns < sections.size()) {
		sc.skinRes("imgs/r_enabled.png")->blit(&s, width - 10, 0);
	} else {
		sc.skinRes("imgs/r_disabled.png")->blit(&s, width - 10, 0);
	}
	for (uint i = iFirstDispSection; i < sections.size() && i < iFirstDispSection + linkColumns; i++) {
		string sectionIcon = "skin:sections/" + sections[i] + ".png";
		int x = (i - iFirstDispSection) * linkWidth + sectionsCoordX;
		if (i == (uint)iSection) {
			s.box(x, 0, linkWidth, topBarHeight, selectionBgColor);
		}
		x += linkWidth / 2;
		if (sc.exists(sectionIcon)) {
			sc[sectionIcon]->blit(&s, x - 16, sectionLinkPadding, 32, 32);
		} else {
			sc.skinRes("icons/section.png")->blit(&s, x - 16, sectionLinkPadding);
		}
		s.write(&font, sections[i], x, topBarHeight - sectionLinkPadding, Font::HAlignCenter, Font::VAlignBottom);
	}

	vector<Link*> &sectionLinks = links[iSection];
	const uint numLinks = sectionLinks.size();
	gmenu2x->drawScrollBar(
			linkRows, (numLinks + linkColumns - 1) / linkColumns, iFirstDispRow,
			topBarHeight + 1, height - topBarHeight - bottomBarHeight - 2);

	//Links
	const uint linksPerPage = linkColumns * linkRows;
	const int linkSpacingX = (width - 10 - linkColumns * linkWidth) / linkColumns;
	const int linkMarginX = (
			width - linkWidth * linkColumns - linkSpacingX * (linkColumns - 1)
			) / 2;
	const int linkSpacingY = (height - 35 - topBarHeight - linkRows * linkHeight) / linkRows;
	for (uint i = iFirstDispRow * linkColumns; i < iFirstDispRow * linkColumns + linksPerPage && i < numLinks; i++) {
		const int ir = i - iFirstDispRow * linkColumns;
		const int x = linkMarginX + (ir % linkColumns) * (linkWidth + linkSpacingX);
		const int y = ir / linkColumns * (linkHeight + linkSpacingY) + topBarHeight + 2;
		sectionLinks.at(i)->setPosition(x, y);

		if (i == (uint)iLink) {
			sectionLinks.at(i)->paintHover();
		}

		sectionLinks.at(i)->paint();
	}

	if (selLink()) {
		s.write(&font, selLink()->getDescription(),
				width / 2, height - bottomBarHeight + 2,
				Font::HAlignCenter, Font::VAlignBottom);
	}
}

void Menu::handleTS() {
	ConfIntHash &skinConfInt = gmenu2x->skinConfInt;
	const int topBarHeight = skinConfInt["topBarHeight"];
	const int linkWidth = skinConfInt["linkWidth"];
	const int screenWidth = gmenu2x->resX;
	const uint linkColumns = gmenu2x->linkColumns, linkRows = gmenu2x->linkRows;

	SDL_Rect re = {
		0, 0,
		static_cast<Uint16>(screenWidth), static_cast<Uint16>(topBarHeight)
	};
	if (ts.pressed() && ts.inRect(re)) {
		re.w = linkWidth;
		uint sectionsCoordX = (screenWidth - constrain((uint)sections.size(), 0, linkColumns) * linkWidth) / 2;
		for (uint i = iFirstDispSection; !ts.handled() && i < sections.size() && i < iFirstDispSection + linkColumns; i++) {
			re.x = (i - iFirstDispSection) * re.w + sectionsCoordX;

			if (ts.inRect(re)) {
				setSectionIndex(i);
				ts.setHandled();
			}
		}
	}

	const uint linksPerPage = linkColumns * linkRows;
	uint i = iFirstDispRow * linkColumns;
	while (i < (iFirstDispRow * linkColumns) + linksPerPage && i < sectionLinks()->size()) {
		if (sectionLinks()->at(i)->isPressed()) {
			setLinkIndex(i);
		}
		if (sectionLinks()->at(i)->handleTS()) {
			i = sectionLinks()->size();
		}
		i++;
	}
}

/*====================================
   SECTION MANAGEMENT
  ====================================*/
void Menu::freeLinks() {
	for (vector< vector<Link*> >::iterator section = links.begin(); section<links.end(); section++)
		for (vector<Link*>::iterator link = section->begin(); link<section->end(); link++)
			delete *link;
}

vector<Link*> *Menu::sectionLinks(int i) {
	if (i<0 || i>(int)links.size())
		i = selSectionIndex();

	if (i<0 || i>(int)links.size())
		return NULL;

	return &links[i];
}

void Menu::decSectionIndex() {
	setSectionIndex(iSection-1);
}

void Menu::incSectionIndex() {
	setSectionIndex(iSection+1);
}

int Menu::selSectionIndex() {
	return iSection;
}

const string &Menu::selSection() {
	return sections[iSection];
}

void Menu::setSectionIndex(int i) {
	if (i<0)
		i=sections.size()-1;
	else if (i>=(int)sections.size())
		i=0;
	iSection = i;

	if (i>(int)iFirstDispSection+2)
		iFirstDispSection = i-2;
	else if (i<(int)iFirstDispSection)
		iFirstDispSection = i;

	iLink = 0;
	iFirstDispRow = 0;
}

/*====================================
   LINKS MANAGEMENT
  ====================================*/
bool Menu::addActionLink(uint section, const string &title, function_t action, const string &description, const string &icon) {
	if (section>=sections.size()) return false;

	Link *link = new Link(gmenu2x, ts, action);
	link->setSize(gmenu2x->skinConfInt["linkWidth"], gmenu2x->skinConfInt["linkHeight"]);
	link->setTitle(title);
	link->setDescription(description);
	if (gmenu2x->sc.exists(icon) || (icon.substr(0,5)=="skin:" && !gmenu2x->sc.getSkinFilePath(icon.substr(5,icon.length())).empty()) || fileExists(icon))
	link->setIcon(icon);

	sectionLinks(section)->push_back(link);
	return true;
}

bool Menu::addLink(string path, string file, string section) {
	if (section=="")
		section = selSection();
	else if (find(sections.begin(),sections.end(),section)==sections.end()) {
		//section directory doesn't exists
		if (!addSection(section))
			return false;
	}

	//strip the extension from the filename
	string title = file;
	string::size_type pos = title.rfind(".");
	if (pos!=string::npos && pos>0) {
		string ext = title.substr(pos, title.length());
		transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		title = title.substr(0, pos);
	}

	string linkpath = GMenu2X::getHome() + "/sections";
	if (!fileExists(linkpath))
		mkdir(linkpath.c_str(), 0755);

	linkpath = GMenu2X::getHome() + "/sections/" + section;
	if (!fileExists(linkpath))
		mkdir(linkpath.c_str(), 0755);

	linkpath += "/" + title;
	int x=2;
	while (fileExists(linkpath)) {
		stringstream ss;
		linkpath = "";
		ss << x;
		ss >> linkpath;
		linkpath = GMenu2X::getHome()+"/sections/"+section+"/"+title+linkpath;
		x++;
	}

	INFO("Adding link: '%s'\n", linkpath.c_str());

	if (path[path.length()-1]!='/') path += "/";
	//search for a manual
	pos = file.rfind(".");
	string exename = path+file.substr(0,pos);
	string manual = "";
	if (fileExists(exename+".man.png")) {
		manual = exename+".man.png";
	} else if (fileExists(exename+".man.jpg")) {
		manual = exename+".man.jpg";
	} else if (fileExists(exename+".man.jpeg")) {
		manual = exename+".man.jpeg";
	} else if (fileExists(exename+".man.bmp")) {
		manual = exename+".man.bmp";
	} else if (fileExists(exename+".man.txt")) {
		manual = exename+".man.txt";
	} else {
		//scan directory for a file like *readme*
		FileLister fl(path, false);
		fl.setFilter(".txt");
		fl.browse();
		bool found = false;
		for (uint x=0; x<fl.size() && !found; x++) {
			string lcfilename = fl[x];

			if (lcfilename.find("readme") != string::npos) {
				found = true;
				manual = path+fl.getFiles()[x];
			}
		}
	}

	INFO("Manual: '%s'\n", manual.c_str());

	string shorttitle=title, description="", exec=path+file, icon="";
	if (fileExists(exename+".png")) icon = exename+".png";

	//Reduce title lenght to fit the link width
	if (gmenu2x->font->getTextWidth(shorttitle)>gmenu2x->skinConfInt["linkWidth"]) {
		while (gmenu2x->font->getTextWidth(shorttitle+"..")>gmenu2x->skinConfInt["linkWidth"])
			shorttitle = shorttitle.substr(0,shorttitle.length()-1);
		shorttitle += "..";
	}

	ofstream f(linkpath.c_str());
	if (f.is_open()) {
		f << "title=" << shorttitle << endl;
		f << "exec=" << exec << endl;
		if (!description.empty()) f << "description=" << description << endl;
		if (!icon.empty()) f << "icon=" << icon << endl;
		if (!manual.empty()) f << "manual=" << manual << endl;
		f.close();
 		sync();
		int isection = find(sections.begin(),sections.end(),section) - sections.begin();
		if (isection>=0 && isection<(int)sections.size()) {

			INFO("Section: '%s(%i)'\n", sections[isection].c_str(), isection);

			LinkApp* link = new LinkApp(gmenu2x, ts, gmenu2x->input, linkpath.c_str());
			link->setSize(gmenu2x->skinConfInt["linkWidth"],gmenu2x->skinConfInt["linkHeight"]);
			links[isection].push_back( link );
		}
	} else {

		ERROR("Error while opening the file '%s' for write.\n", linkpath.c_str());

		return false;
	}

	return true;
}

bool Menu::addSection(const string &sectionName) {
	string sectiondir = GMenu2X::getHome() + "/sections";
	if (!fileExists(sectiondir))
		mkdir(sectiondir.c_str(), 0755);

	sectiondir = sectiondir + "/" + sectionName;
	if (mkdir(sectiondir.c_str(), 0755) == 0) {
		sections.push_back(sectionName);
		vector<Link*> ll;
		links.push_back(ll);
		return true;
	}
	return false;
}

void Menu::deleteSelectedLink()
{
	bool icon_used = false;
	string iconpath = selLink()->getIconPath();

	INFO("Deleting link '%s'\n", selLink()->getTitle().c_str());

	if (selLinkApp()!=NULL)
		unlink(selLinkApp()->getFile().c_str());
	sectionLinks()->erase( sectionLinks()->begin() + selLinkIndex() );
	setLinkIndex(selLinkIndex());

	for (vector< vector<Link*> >::iterator section = links.begin();
				!icon_used && section<links.end(); section++)
		for (vector<Link*>::iterator link = section->begin();
					!icon_used && link<section->end(); link++)
			icon_used = !iconpath.compare((*link)->getIconPath());

	if (!icon_used)
	  gmenu2x->sc.del(iconpath);
}

void Menu::deleteSelectedSection() {
	INFO("Deleting section '%s'\n", selSection().c_str());

	gmenu2x->sc.del("sections/"+selSection()+".png");
	links.erase( links.begin()+selSectionIndex() );
	sections.erase( sections.begin()+selSectionIndex() );
	setSectionIndex(0); //reload sections
}

bool Menu::linkChangeSection(uint linkIndex, uint oldSectionIndex, uint newSectionIndex) {
	if (oldSectionIndex<sections.size() && newSectionIndex<sections.size() && linkIndex<sectionLinks(oldSectionIndex)->size()) {
		sectionLinks(newSectionIndex)->push_back( sectionLinks(oldSectionIndex)->at(linkIndex) );
		sectionLinks(oldSectionIndex)->erase( sectionLinks(oldSectionIndex)->begin()+linkIndex );
		//Select the same link in the new position
		setSectionIndex(newSectionIndex);
		setLinkIndex(sectionLinks(newSectionIndex)->size()-1);
		return true;
	}
	return false;
}

void Menu::linkLeft() {
	if (iLink%gmenu2x->linkColumns == 0)
		setLinkIndex( sectionLinks()->size()>iLink+gmenu2x->linkColumns-1 ? iLink+gmenu2x->linkColumns-1 : sectionLinks()->size()-1 );
	else
		setLinkIndex(iLink-1);
}

void Menu::linkRight() {
	if (iLink%gmenu2x->linkColumns == (gmenu2x->linkColumns-1) || iLink == (int)sectionLinks()->size()-1)
		setLinkIndex(iLink-iLink%gmenu2x->linkColumns);
	else
		setLinkIndex(iLink+1);
}

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

void Menu::linkUp() {
	int l = iLink-gmenu2x->linkColumns;
	if (l<0) {
		unsigned int rows;
		rows = DIV_ROUND_UP(sectionLinks()->size(), gmenu2x->linkColumns);
		l = (rows*gmenu2x->linkColumns)+l;
		if (l >= (int)sectionLinks()->size())
			l -= gmenu2x->linkColumns;
	}
	setLinkIndex(l);
}

void Menu::linkDown() {
	uint l = iLink+gmenu2x->linkColumns;
	if (l >= sectionLinks()->size()) {
		unsigned int rows, curCol;
		rows = DIV_ROUND_UP(sectionLinks()->size(), gmenu2x->linkColumns);
		curCol = DIV_ROUND_UP(iLink + 1, gmenu2x->linkColumns);
		if (rows > curCol)
			l = sectionLinks()->size()-1;
		else
			l %= gmenu2x->linkColumns;
	}
	setLinkIndex(l);
}

int Menu::selLinkIndex() {
	return iLink;
}

Link *Menu::selLink() {
	if (sectionLinks()->size()==0) return NULL;
	return sectionLinks()->at(iLink);
}

LinkApp *Menu::selLinkApp() {
	return dynamic_cast<LinkApp*>(selLink());
}

void Menu::setLinkIndex(int i) {
	if (i<0)
		i=sectionLinks()->size()-1;
	else if (i>=(int)sectionLinks()->size())
		i=0;

	if (i>=(int)(iFirstDispRow*gmenu2x->linkColumns+gmenu2x->linkColumns*gmenu2x->linkRows))
		iFirstDispRow = i/gmenu2x->linkColumns-gmenu2x->linkRows+1;
	else if (i<(int)(iFirstDispRow*gmenu2x->linkColumns))
		iFirstDispRow = i/gmenu2x->linkColumns;

	iLink = i;
}

#ifdef HAVE_LIBOPK
void Menu::openPackagesFromDir(std::string path)
{
	if (access(path.c_str(), F_OK))
		return;

	DEBUG("Opening packages from directory: %s\n", path.c_str());
	readPackages(path);
#ifdef ENABLE_INOTIFY
	monitors.push_back(new Monitor(path.c_str()));
#endif
}

void Menu::openPackage(std::string path, bool order)
{
	/* First try to remove existing links of the same OPK
	 * (needed for instance when an OPK is modified) */
	removePackageLink(path);

	struct OPK *opk = opk_open(path.c_str());
	if (!opk) {
		ERROR("Unable to open OPK %s\n", path.c_str());
		return;
	}

	for (;;) {
		unsigned int i;
		bool has_metadata = false;
		LinkApp *link;

		for (;;) {
			string::size_type pos;
			const char *name;
			int ret = opk_open_metadata(opk, &name);
			if (ret < 0) {
				ERROR("Error while loading meta-data\n");
				break;
			} else if (!ret)
			  break;

			/* Strip .desktop */
			string metadata(name);
			pos = metadata.rfind('.');
			metadata = metadata.substr(0, pos);

			/* Keep only the platform name */
			pos = metadata.rfind('.');
			metadata = metadata.substr(pos + 1);

			if (!metadata.compare(PLATFORM) || !metadata.compare("all")) {
				has_metadata = true;
				break;
			}
		}

		if (!has_metadata)
		  break;

		link = new LinkApp(gmenu2x, ts, gmenu2x->input, path.c_str(), opk);
		link->setSize(gmenu2x->skinConfInt["linkWidth"], gmenu2x->skinConfInt["linkHeight"]);

		addSection(link->getCategory());
		for (i = 0; i < sections.size(); i++) {
			if (sections[i] == link->getCategory()) {
				links[i].push_back(link);
				break;
			}
		}
	}

	opk_close(opk);

	if (order)
		orderLinks();
}

void Menu::readPackages(std::string parentDir)
{
	DIR *dirp;
	struct dirent *dptr;
	vector<string> linkfiles;

	dirp = opendir(parentDir.c_str());
	if (!dirp)
		return;

	while ((dptr = readdir(dirp))) {
		char *c;

		if (dptr->d_type != DT_REG)
			continue;

		c = strrchr(dptr->d_name, '.');
		if (!c) /* File without extension */
			continue;

		if (strcasecmp(c + 1, "opk"))
			continue;

		if (dptr->d_name[0] == '.') {
			// Ignore hidden files.
			// Mac OS X places these on SD cards, probably to store metadata.
			continue;
		}

		openPackage(parentDir + '/' + dptr->d_name, false);
	}

	closedir(dirp);
	orderLinks();
}

#ifdef ENABLE_INOTIFY
/* Remove all links that correspond to the given path.
 * If "path" is a directory, it will remove all links that
 * correspond to an OPK present in the directory. */
void Menu::removePackageLink(std::string path)
{
	for (vector< vector<Link*> >::iterator section = links.begin();
				section < links.end(); section++) {
		for (vector<Link*>::iterator link = section->begin();
					link < section->end(); link++) {
			LinkApp *app = dynamic_cast<LinkApp *> (*link);
			if (!app || !app->isOpk() || app->getOpkFile().empty())
				continue;

			if (app->getOpkFile().compare(0, path.size(), path) == 0) {
				DEBUG("Removing link corresponding to package %s\n",
							app->getOpkFile().c_str());
				section->erase(link);
				if (section - links.begin() == iSection
							&& iLink == (int) section->size())
					setLinkIndex(iLink - 1);
				link--;
			}
		}
	}

	/* Remove registered monitors */
	for (vector<Monitor *>::iterator it = monitors.begin();
				it < monitors.end(); it++) {
		if ((*it)->getPath().compare(0, path.size(), path) == 0) {
			delete (*it);
			monitors.erase(it);
		}
	}
}
#endif
#endif

void Menu::readLinksOfSection(std::string path, std::vector<std::string> &linkfiles)
{
	DIR *dirp;
	struct dirent *dptr;

	if ((dirp = opendir(path.c_str())) == NULL) return;

	while ((dptr = readdir(dirp))) {
		if (dptr->d_type != DT_REG) continue;
		string filepath = path + "/" + dptr->d_name;
		linkfiles.push_back(filepath);
	}

	closedir(dirp);
}

static bool compare_links(Link *a, Link *b)
{
	return a->getTitle().compare(b->getTitle()) <= 0;
}

void Menu::orderLinks()
{
	for (std::vector< std::vector<Link *> >::iterator section = links.begin();
				section < links.end(); section++)
		if (section->size() > 1)
			std::sort(section->begin(), section->end(), compare_links);
}

void Menu::readLinks() {
	vector<string> linkfiles;

	iLink = 0;
	iFirstDispRow = 0;
	string filepath;

	for (uint i=0; i<links.size(); i++) {
		links[i].clear();
		linkfiles.clear();

		int correct = (i>sections.size() ? iSection : i);

		readLinksOfSection(GMENU2X_SYSTEM_DIR "/sections/"
		  + sections[correct], linkfiles);

		readLinksOfSection(GMenu2X::getHome() + "/sections/"
		  + sections[correct], linkfiles);

		sort(linkfiles.begin(), linkfiles.end(),case_less());
		for (uint x=0; x<linkfiles.size(); x++) {
			LinkApp *link = new LinkApp(gmenu2x, ts, gmenu2x->input, linkfiles[x].c_str());
			link->setSize(gmenu2x->skinConfInt["linkWidth"], gmenu2x->skinConfInt["linkHeight"]);
			if (link->targetExists())
				links[i].push_back(link);
			else
				delete link;
		}
	}
}

void Menu::renameSection(int index, const string &name) {
	sections[index] = name;
}
