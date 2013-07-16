#ifdef ENABLE_INOTIFY
#include "debug.h"

#include <dirent.h>
#include <pthread.h>
#include <SDL.h>
#include <signal.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "monitor.h"

static void * inotify_thd(void *p)
{
	const char *path = (const char *) p;
	int wd, fd;

	DEBUG("Starting inotify thread for path %s...\n", path);

	fd = inotify_init();
	if (fd == -1) {
		ERROR("Unable to start inotify\n");
		return NULL;
	}

	wd = inotify_add_watch(fd, path, IN_MOVED_FROM | IN_MOVED_TO |
				IN_CLOSE_WRITE | IN_DELETE);
	if (wd == -1) {
		ERROR("Unable to add inotify watch\n");
		close(fd);
		return NULL;
	}

	DEBUG("Starting watching directory %s\n", path);

	for (;;) {
		size_t len = sizeof(struct inotify_event) + NAME_MAX + 1;
		struct inotify_event event;
		char buf[256];

		read(fd, &event, len);
		sprintf(buf, "%s/%s", path, event.name);

		/* Don't bother other files than OPKs */
		len = strlen(event.name);
		if (len < 5 || strncmp(event.name + len - 4, ".opk", 4))
			continue;

		SDL_UserEvent e = {
			.type = SDL_USEREVENT,
			.code = (int) (event.mask & (IN_MOVED_TO | IN_CLOSE_WRITE)),
			.data1 = strdup(buf),
			.data2 = NULL,
		};

		/* Inject an user event, that will be handled as a "repaint"
		 * event by the InputManager */
		SDL_PushEvent((SDL_Event *) &e);
	}
}

Monitor::Monitor(std::string path) : path(path)
{
	pthread_create(&thd, NULL, inotify_thd, (void *) path.c_str());
}

Monitor::~Monitor(void)
{
	pthread_kill(thd, SIGINT);
}
#endif
