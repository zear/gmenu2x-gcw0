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
	void resetTimer();
	int update();
	unsigned int clockCallback(unsigned int timeout);

	SDL_TimerID timer;
	unsigned int timeout_startms;
	int minutes, hours;
};

#endif /* __CLOCK_H__ */
