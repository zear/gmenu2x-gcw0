// Various authors.
// License: GPL version 2 or later.

#include "background.h"

#include "gmenu2x.h"


Background::Background(GMenu2X &gmenu2x)
	: gmenu2x(gmenu2x)
	, battery(gmenu2x.sc)
{
}

void Background::paint(Surface &s) {
	Font &font = *gmenu2x.font;
	SurfaceCollection &sc = gmenu2x.sc;

	sc["bgmain"]->blit(&s, 0, 0);

	s.write(&font, clock.getTime(),
			s.width() / 2, gmenu2x.bottomBarTextY,
			Font::HAlignCenter, Font::VAlignMiddle);

	battery.getIcon().blit(&s, s.width() - 19, gmenu2x.bottomBarIconY);
}

bool Background::handleButtonPress(InputManager::Button button) {
	switch (button) {
		case InputManager::CANCEL:
			gmenu2x.showHelpPopup();
			return true;
		case InputManager::SETTINGS:
			gmenu2x.showSettings();
			return true;
		case InputManager::MENU:
			gmenu2x.contextMenu();
			return true;
		default:
			return false;
	}
}

bool Background::handleTouchscreen(Touchscreen &/*ts*/) {
	return false;
}
