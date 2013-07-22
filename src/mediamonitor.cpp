#ifdef ENABLE_INOTIFY
#include <sys/inotify.h>
#include <SDL/SDL.h>
#include <unistd.h>

#include "debug.h"
#include "inputmanager.h"
#include "mediamonitor.h"

MediaMonitor::MediaMonitor(std::string dir) :
	Monitor(dir, IN_MOVE | IN_DELETE | IN_CREATE | IN_ONLYDIR)
{
}

bool MediaMonitor::event_accepted(
			struct inotify_event &event __attribute__((unused)))
{
	return true;
}

void MediaMonitor::inject_event(bool is_add, const char *path)
{
	SDL_UserEvent e = {
		.type = SDL_USEREVENT,
		.code = is_add ? OPEN_PACKAGES_FROM_DIR : REMOVE_LINKS,
		.data1 = strdup(path),
		.data2 = NULL,
	};

	/* Sleep for a bit, to ensure that the media will be mounted
	 * on the mountpoint before we start looking for OPKs */
	sleep(1);

	DEBUG("MediaMonitor: Injecting event code %i\n", e.code);

	/* Inject an user event, that will be handled as a "repaint"
	 * event by the InputManager */
	SDL_PushEvent((SDL_Event *) &e);
}

#endif /* ENABLE_INOTIFY */
