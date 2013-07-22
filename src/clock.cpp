#include <string>
#include <sys/time.h>

#include "clock.h"
#include "debug.h"
#include "inputmanager.h"

Clock *Clock::instance = NULL;

static void notify(void)
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

static Uint32 clockCallback(Uint32 timeout, void *d)
{
	unsigned int *old_ticks = (unsigned int *) d;
	unsigned int new_ticks = SDL_GetTicks();

	if (new_ticks > *old_ticks + timeout + 1000) {
		DEBUG("Suspend occured, restarting timer\n");
		*old_ticks = new_ticks;
		return timeout;
	}

	Clock::getInstance()->resetTimer();
	notify();
	return 60000;
}

std::string &Clock::getTime(bool is24)
{
	char buf[9];
	int h = hours;
	bool pm = hours >= 12;

	if (!is24 && pm)
		h -= 12;

	sprintf(buf, "%02i:%02i%s", h, minutes, is24 ? "" : (pm ? "pm" : "am"));
	str = buf;
	return str;
}

int Clock::update(void)
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

void Clock::resetTimer(void)
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
	timer = SDL_AddTimer(timeout, clockCallback, &timeout_startms);
	if (timer == NULL)
		ERROR("Could not initialize SDLTimer: %s\n", SDL_GetError());
}

Clock::Clock(void)
{
	SDL_InitSubSystem(SDL_INIT_TIMER);
	tzset();

	int sec = update();
	addTimer((60 - sec) * 1000);
}

Clock::~Clock()
{
	SDL_RemoveTimer(timer);
	SDL_QuitSubSystem(SDL_INIT_TIMER);
	instance = NULL;
}

Clock *Clock::getInstance(void)
{
	if (!instance)
		instance = new Clock();
	return instance;
}

bool Clock::isRunning(void)
{
	return instance != NULL;
}
