#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include "button.h"

#include <string>

class GMenu2X;
class Surface;

class IconButton : private Button {
public:
	IconButton(GMenu2X *gmenu2x, Touchscreen &ts,
			const std::string &icon, const std::string &label = "");
	virtual ~IconButton() {};

	virtual void paint();

	virtual void setPosition(int x, int y);

	void setAction(function_t action);

	// Expose some Button functionality:
	SDL_Rect getRect() { return Button::getRect(); }
	bool handleTS() { return Button::handleTS(); }

private:
	void recalcSize();

	GMenu2X *gmenu2x;
	std::string icon, label;
	SDL_Rect iconRect, labelRect;

	Surface *iconSurface;
};

#endif
