#ifndef __MONITOR_H__
#define __MONITOR_H__
#ifdef ENABLE_INOTIFY

#include <pthread.h>
#include <string>
#include <sys/inotify.h>

class Monitor {
public:
	Monitor(std::string path, unsigned int flags = IN_MOVE |
				IN_CLOSE_WRITE | IN_DELETE | IN_CREATE);
	virtual ~Monitor();

	int run(void);

private:
	std::string path;
	pthread_t thd;

protected:
	unsigned int mask;
	virtual bool event_accepted(struct inotify_event &event);
	virtual void inject_event(bool is_add, const char *path);
};

#endif
#endif /* __MONITOR_H__ */
