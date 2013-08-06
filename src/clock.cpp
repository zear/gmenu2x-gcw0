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
	static unsigned int clockCallbackFunc(Clock *clock, unsigned int timeout)
	{
		return clock->clockCallback(timeout);
	}
	friend Uint32 clockCallbackFunc(Uint32 timeout, void *d);
};

extern "C" Uint32 clockCallbackFunc(Uint32 timeout, void *d)
{
	return Clock::Forwarder::clockCallbackFunc(static_cast<Clock *>(d), timeout);
}

unsigned int Clock::clockCallback(unsigned int timeout)
{
	unsigned int now_ticks = SDL_GetTicks();

	if (now_ticks > timeout_startms + timeout + 1000) {
		DEBUG("Suspend occured, restarting timer\n");
		timeout_startms = now_ticks;
		return timeout;
	}

	resetTimer();
	notify();
	return 60000;
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

int Clock::update()
{
	struct timeval tv;
	struct tm result;
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &result);
	minutes = result.tm_min;
	hours = result.tm_hour;
	DEBUG("Time updated: %02i:%02i\n", hours, minutes);
	return result.tm_sec;
}

void Clock::resetTimer()
{
	SDL_RemoveTimer(timer);
	timer = NULL;

	int secs = update();
	addTimer((60 - secs) * 1000);
}

void Clock::addTimer(int timeout)
{
	if (timeout < 1000 || timeout > 60000)
		timeout = 60000;

	timeout_startms = SDL_GetTicks();
	timer = SDL_AddTimer(timeout, clockCallbackFunc, this);
	if (timer == NULL)
		ERROR("Could not initialize SDLTimer: %s\n", SDL_GetError());
}

Clock::Clock()
{
	tzset();

	int sec = update();
	addTimer((60 - sec) * 1000);
}

Clock::~Clock()
{
	SDL_RemoveTimer(timer);
}
