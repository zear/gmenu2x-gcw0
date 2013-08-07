#include "clock.h"

#include "debug.h"
#include "inputmanager.h"

#include <sys/time.h>


static void notify()
{
	SDL_UserEvent e = {
		.type = SDL_USEREVENT,
		.code = REPAINT_MENU,
		.data1 = NULL,
		.data2 = NULL,
	};

	/* Inject an user event, that will be handled as a "repaint"
	 * event by the InputManager */
	SDL_PushEvent((SDL_Event *) &e);
}

extern "C" Uint32 clockCallbackFunc(Uint32 timeout, void *d);

class Clock::Forwarder {
	static unsigned int clockCallbackFunc(Clock *clock)
	{
		return clock->clockCallback();
	}
	friend Uint32 clockCallbackFunc(Uint32 timeout, void *d);
};

extern "C" Uint32 clockCallbackFunc(Uint32 timeout, void *d)
{
	return Clock::Forwarder::clockCallbackFunc(static_cast<Clock *>(d));
}

unsigned int Clock::clockCallback()
{
	unsigned int ms = update();
	notify();
	return ms;
}

std::string Clock::getTime(bool is24)
{
	char buf[9];
	int h = hours;
	bool pm = hours >= 12;

	if (!is24 && pm)
		h -= 12;

	sprintf(buf, "%02i:%02i%s", h, minutes, is24 ? "" : (pm ? "pm" : "am"));
	return std::string(buf);
}

unsigned int Clock::update()
{
	struct timeval tv;
	struct tm result;
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &result);
	minutes = result.tm_min;
	hours = result.tm_hour;
	DEBUG("Time updated: %02i:%02i:%02i\n", hours, minutes, result.tm_sec);

	// Compute number of milliseconds to next minute boundary.
	// We don't need high precision, but it is important that any deviation is
	// past the minute mark, so the fetched hour and minute number belong to
	// the freshly started minute.
	// Clamping it at 1 sec both avoids overloading the system in case our
	// computation goes haywire and avoids passing 0 to SDL, which would stop
	// the recurring timer.
	return std::max(1, (60 - result.tm_sec)) * 1000;
}

void Clock::addTimer(int timeout)
{
	timer = SDL_AddTimer(timeout, clockCallbackFunc, this);
	if (timer == NULL)
		ERROR("Could not initialize SDLTimer: %s\n", SDL_GetError());
}

Clock::Clock()
{
	tzset();

	unsigned int ms = update();
	addTimer(ms);
}

Clock::~Clock()
{
	SDL_RemoveTimer(timer);
}
