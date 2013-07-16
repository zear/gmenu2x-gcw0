#ifndef __MONITOR_H__
#define __MONITOR_H__

#include <string>
#include <pthread.h>

class Monitor {
public:
	Monitor(std::string path);
	~Monitor();

private:
	std::string path;
	pthread_t thd;
};

#endif /* __MONITOR_H__ */
