#include "button.h"
#include "delegate.h"
#include "gmenu2x.h"

using namespace std;

Button::Button(Touchscreen &ts_, bool doubleClick_)
	: action(BIND(&Button::voidAction))
	, rect((SDL_Rect) { 0, 0, 0, 0 })
	, ts(ts_)
	, doubleClick(doubleClick_)
	, lastTick(0)
{
}

bool Button::isPressed() {
	return ts.pressed() && ts.inRect(rect);
}

bool Button::isReleased() {
	return ts.released() && ts.inRect(rect);
}

bool Button::handleTS() {
	if (isReleased()) {
		if (doubleClick) {
			int tickNow = SDL_GetTicks();
			if (tickNow - lastTick < 400) {
				ts.setHandled();
				action();
			}
			lastTick = tickNow;
		} else {
			ts.setHandled();
			action();
		}
		return true;
	}
	return false;
}

SDL_Rect Button::getRect() {
	return rect;
}

void Button::setSize(int w, int h) {
	rect.w = w;
	rect.h = h;
}

void Button::setPosition(int x, int y) {
	rect.x = x;
	rect.y = y;
}
