#include "clock.h"

#include "debug.h"
#include "inputmanager.h"

#include <SDL.h>
#include <sys/time.h>


class Clock::Timer {
public:
	Timer();
	~Timer();
	void start();
	void getTime(unsigned int &hours, unsigned int &minutes);
	unsigned int callback();

private:
	unsigned int update();

	SDL_TimerID timerID;
	int minutes, hours;
};

static std::weak_ptr<Clock::Timer> globalTimer;

/**
 * Gets the global timer instance, or create it if it doesn't exist already.
 * This function is not thread safe: only one thread at a time may call it.
 */
static std::shared_ptr<Clock::Timer> globalTimerInstance()
{
	std::shared_ptr<Clock::Timer> timer = globalTimer.lock();
	if (timer) {
		return timer;
	} else {
		// Note: Separate start method is necessary because globalTimer must
		//       be written before callbacks can occur.
		timer.reset(new Clock::Timer());
		globalTimer = timer;
		timer->start();
		return timer;
	}
}

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

extern "C" Uint32 callbackFunc(Uint32 /*timeout*/, void */*d*/)
{
	std::shared_ptr<Clock::Timer> timer = globalTimer.lock();
	return timer ? timer->callback() : 0;
}

Clock::Timer::Timer()
	: timerID(NULL)
{
	tzset();
}

Clock::Timer::~Timer()
{
	if (timerID) {
		SDL_RemoveTimer(timerID);
	}
}

void Clock::Timer::start()
{
	if (timerID) {
		ERROR("SDL timer was already started\n");
		return;
	}
	unsigned int ms = update();
	timerID = SDL_AddTimer(ms, callbackFunc, this);
	if (!timerID) {
		ERROR("Could not initialize SDL timer: %s\n", SDL_GetError());
	}
}

void Clock::Timer::getTime(unsigned int &hours, unsigned int &minutes)
{
	hours = this->hours;
	minutes = this->minutes;
}

unsigned int Clock::Timer::update()
{
	struct timeval tv;
	struct tm result;
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &result);
	// TODO: Store both in a single 32-bit write?
	minutes = result.tm_min;
	hours = result.tm_hour;
	DEBUG("Time updated: %02i:%02i:%02i\n", hours, minutes, result.tm_sec);

	// Compute number of milliseconds to next minute boundary.
	// We don't need high precision, but it is important that any deviation is
	// past the minute mark, so the fetched hour and minute number belong to
	// the freshly started minute.
	// TODO: Does the SDL timer in fact guarantee we're never called early?
	//       "ms = t->interval - SDL_TIMESLICE;" worries me.
	// Clamping it at 1 sec both avoids overloading the system in case our
	// computation goes haywire and avoids passing 0 to SDL, which would stop
	// the recurring timer.
	return std::max(1, (60 - result.tm_sec)) * 1000;
}

unsigned int Clock::Timer::callback()
{
	unsigned int ms = update();
	notify();
	// TODO: SDL timer forgets adjusted interval if a timer was inserted or
	//       removed during the callback. So we should either fix that bug
	//       in SDL or ensure we don't insert/remove timers at runtime.
	//       The blanking timer is inserted/removed quite a lot at time moment,
	//       but it could be reprogrammed to adjust the interval instead.
	return ms;
}

std::string Clock::getTime(bool is24)
{
	unsigned int hours, minutes;
	timer->getTime(hours, minutes);

	bool pm = hours >= 12;
	if (!is24 && pm)
		hours -= 12;

	char buf[9];
	sprintf(buf, "%02i:%02i%s", hours, minutes, is24 ? "" : (pm ? "pm" : "am"));
	return std::string(buf);
}

Clock::Clock()
	: timer(globalTimerInstance())
{
}
