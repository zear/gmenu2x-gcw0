#ifndef __BATTERY_H__
#define __BATTERY_H__

#include <string>

class Surface;
class SurfaceCollection;


/**
 * Keeps track of the battery status.
 */
class Battery {
public:
	Battery(SurfaceCollection &sc);

	/**
	 * Gets the icon that reflects the current battery status.
	 */
	const Surface &getIcon();

private:
	void update();

	SurfaceCollection &sc;
	std::string iconPath;
	unsigned int lastUpdate;
};

#endif /* __BATTERY_H__ */
