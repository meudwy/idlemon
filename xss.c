
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

#include "idlemon.h"


static Display *dpy = NULL;
static XScreenSaverInfo *info = NULL;


void
xss_init(void)
{
	int event_base;
	int error_base;

	if ((dpy = XOpenDisplay(NULL)) == NULL) {
		log_fatal("xss: failed to open display");
	}

	if (XScreenSaverQueryExtension(dpy, &event_base, &error_base) == 0) {
		log_fatal("xss: extension not enabled");
	}

	if ((info = XScreenSaverAllocInfo()) == NULL) {
		log_fatal("xss: out of memory");
	}
}

void
xss_deinit(void)
{
	XFree(info);
	XCloseDisplay(dpy);
}

struct xss
xss_query(void)
{
	if (XScreenSaverQueryInfo(dpy, XDefaultRootWindow(dpy), info) == 0) {
		log_fatal("xss: query failed");
	}

	return (struct xss){
		.idle = info->idle,
		.active = info->state == ScreenSaverActive,
	};
}

