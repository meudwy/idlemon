#ifndef IDLEMON_H
#define IDLEMON_H

#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>

extern bool color_tty;


enum taskstate {
	TASK_PENDING,
	TASK_STARTED,
	TASK_COMPLETED,
};

struct task {
	struct task *next;
	char **argv;
	unsigned long delay;

	enum taskstate state;
	pid_t pid;
};

void task_process(struct task *task, unsigned long idle, bool idle_reset);
void task_deinit(struct task *task);
void task_destroy(struct task *task);


enum log_level {
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
};

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

char *strltrim(char *s);
char *strrntrim(char *s, size_t len);
char *strntrim(char *s, size_t len);
char *strtolower(char *s);
int strtobool(const char *s);


struct config {
	unsigned long delay;
	struct {
		enum log_level level;
		bool time;
	} log;
	struct task *tasks;
};

#define CONFIG_INIT { \
	.delay = 60000, \
	.log = { \
		.level = LOG_INFO, \
		.time = true, \
	}, \
}

extern struct config config;

bool config_load(const char *filename, struct config *cfg);
bool config_load_and_swap(const char *filename);
void config_deinit(struct config *cfg);


void xss_init(void);
void xss_deinit(void);
unsigned long xss_get_idle(void);

#endif // IDLEMON_H
