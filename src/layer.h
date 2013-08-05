// (c) 2013 Maarten ter Huurne <maarten@treewalker.org>
// License: GPL version 2 or later.

#ifndef LAYER_H
#define LAYER_H

#include "inputmanager.h"

class Surface;
class Touchscreen;


/**
 * Abstract base class for UI layers.
 * A layer handles both painting and input events.
 */
class Layer {
public:
	virtual ~Layer() {}

	/**
	 * Paints this layer on the given surface.
	 */
	virtual void paint(Surface &s) = 0;

	/**
	 * Handles the pressing of the give button.
	 * Returns true iff the button press event was fully handled by this layer.
	 */
	virtual bool handleButtonPress(InputManager::Button button) = 0;

	/**
	 * Handles the touch screen.
	 * Only called if there is a touch screen available.
	 * Returns true iff the touch screen was fully handled by this layer.
	 */
	virtual bool handleTouchscreen(Touchscreen &ts) = 0;

	bool wasDismissed() { return dismissed; }

protected:
	/**
	 * Set this to true to request the layer to be removed from the stack.
	 */
	bool dismissed = false;
};

#endif // LAYER_H
