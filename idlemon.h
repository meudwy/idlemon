#ifndef IDLEMON_H
#define IDLEMON_H

#include <stdarg.h>
#include <stdbool.h>

extern bool color_tty;


enum log_level {
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
};

struct log_config {
	enum log_level level;
	bool time;
};

extern struct log_config log_config;

__attribute__((noreturn))
__attribute__((format(printf, 1, 2)))
void log_fatal(const char *fmt, ...);

__attribute__((format(printf, 1, 2)))
void log_error(const char *fmt, ...);

__attribute__((format(printf, 1, 2)))
void log_warn(const char *fmt, ...);

__attribute__((format(printf, 1, 2)))
void log_info(const char *fmt, ...);

__attribute__((format(printf, 1, 2)))
void log_debug(const char *fmt, ...);

#endif // IDLEMON_H
