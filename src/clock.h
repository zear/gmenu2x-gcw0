#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <string>
#include <SDL.h>

class Clock {
public:
	Clock();
	~Clock();

	std::string getTime(bool is24 = true);

	class Forwarder;
	friend Forwarder;

private:
	void addTimer(int timeout);
	unsigned int update();
	unsigned int clockCallback();

	SDL_TimerID timer;
	int minutes, hours;
};

#endif /* __CLOCK_H__ */
