// Various authors.
// License: GPL version 2 or later.

#include "helppopup.h"

#include "gmenu2x.h"


HelpPopup::HelpPopup(GMenu2X &gmenu2x)
	: gmenu2x(gmenu2x)
{
}

void HelpPopup::paint(Surface &s) {
	Font *font = gmenu2x.font;
	Translator &tr = gmenu2x.tr;
	int helpBoxHeight = 154;

	s.box(10, 50, 300, helpBoxHeight + 4,
			gmenu2x.skinConfColors[COLOR_MESSAGE_BOX_BG]);
	s.rectangle(12, 52, 296, helpBoxHeight,
			gmenu2x.skinConfColors[COLOR_MESSAGE_BOX_BORDER]);
	s.write(font, tr["CONTROLS"], 20, 60);
#if defined(PLATFORM_A320) || defined(PLATFORM_GCW0)
	s.write(font, tr["A: Launch link / Confirm action"], 20, 80);
	s.write(font, tr["B: Show this help menu"], 20, 95);
	s.write(font, tr["L, R: Change section"], 20, 110);
	s.write(font, tr["SELECT: Show contextual menu"], 20, 155);
	s.write(font, tr["START: Show options menu"], 20, 170);
#endif
}

bool HelpPopup::handleButtonPress(InputManager::Button button) {
	if (button == InputManager::CANCEL) {
		dismissed = true;
	}
	return true;
}

bool HelpPopup::handleTouchscreen(Touchscreen &ts) {
	if (ts.pressed()) {
		dismissed = true;
		ts.setHandled();
	}
	return true;
}
