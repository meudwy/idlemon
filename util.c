
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "idlemon.h"

__attribute__((format(printf, 2, 0)))
static void
log_msgv(enum log_level level, const char *fmt, va_list ap)
{
	int e = errno;
	const char *name, *color;
	size_t n;

	switch (level) {
	case LOG_ERROR: name = "ERR"; color = "31"; break;
	case LOG_WARN:  name = "WRN"; color = "33"; break;
	case LOG_INFO:  name = "INF"; color = "39"; break;
	case LOG_DEBUG: name = "DBG"; color = "34"; break;
	}

	if (log_config.time) {
		char buf[32];
		struct tm tm;
		time_t t = time(NULL);

		if (localtime_r(&t, &tm) != NULL &&
				strftime(buf, sizeof(buf), "%Y-%m-%dT%T%z", &tm) > 0) {
			fprintf(stderr, "%s ", buf);
		}
	}

	if (color_tty) {
		fprintf(stderr, "\033[1;%sm%s:\033[0m ", color, name);
	} else {
		fprintf(stderr, "%s: ", name);
	}

	vfprintf(stderr, fmt, ap);

	if ((n = strlen(fmt)) > 0 && fmt[n - 1] == ':') {
		if (color_tty) {
			fprintf(stderr, " \033[31m%s\033[0m", strerror(e));
		} else {
			fprintf(stderr, " %s", strerror(e));
		}
	}

	fputc('\n', stderr);
}

#define IMPL_LOG_FN(NAME, LEVEL, ...) \
	void \
	log_##NAME(const char *fmt, ...) \
	{ \
		va_list ap; \
		if (LEVEL <= log_config.level) { \
			va_start(ap, fmt); \
			log_msgv(LEVEL, fmt, ap); \
			va_end(ap); \
		} \
		__VA_ARGS__ \
	}

IMPL_LOG_FN(fatal, LOG_ERROR, exit(1);)
IMPL_LOG_FN(error, LOG_ERROR, ;)
IMPL_LOG_FN(warn, LOG_WARN, ;)
IMPL_LOG_FN(info, LOG_INFO, ;)
IMPL_LOG_FN(debug, LOG_DEBUG, ;)

