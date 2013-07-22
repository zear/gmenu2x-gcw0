#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <string>
#include <SDL.h>

class Clock {
public:
	static Clock *getInstance();
	~Clock();

	std::string &getTime(bool is24 = true);
	static bool isRunning();
	void resetTimer();

private:
	Clock();
	void addTimer(int timeout);
	int update();

	static Clock *instance;
	SDL_TimerID timer;
	unsigned int timeout_startms;
	int minutes, hours;
	std::string str;
};

#endif /* __CLOCK_H__ */
