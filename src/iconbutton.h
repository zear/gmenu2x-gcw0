#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include "delegate.h"
#include "gmenu2x.h"

#include <SDL.h>
#include <string>

class Surface;
class Touchscreen;


class IconButton {
public:
	IconButton(GMenu2X *gmenu2x, Touchscreen &ts,
			const std::string &icon, const std::string &label = "");

	void setAction(function_t action);

	SDL_Rect getRect() { return rect; }
	void setPosition(int x, int y);

	bool handleTS();

	void paint(Surface *s);
	void paint() { paint(gmenu2x->s); }

private:
	void recalcRects();

	GMenu2X *gmenu2x;
	Touchscreen &ts;
	std::string icon, label;
	function_t action;

	SDL_Rect rect, iconRect, labelRect;
	Surface *iconSurface;
};

#endif
