#ifndef IDLEMON_H
#define IDLEMON_H

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>

extern bool color_tty;

#define TASK_DELAY_XSS ULONG_MAX

struct state {
	unsigned long idle;
	bool xss_active;
};

enum taskstate {
	TASK_PENDING,
	TASK_STARTED,
	TASK_COMPLETED,
};

struct task {
	char *name;
	char **argv;
	unsigned long delay;

	enum taskstate state;
	bool temporary;
	pid_t pid;
};

struct tasklist {
	struct task *entries;
	size_t len;
	size_t cap;
};

bool task_process(struct task *task, const struct state *state, const struct state *prev_state);
struct task *task_clone(struct task *dst, const struct task *src);
void task_deinit(struct task *task);
bool tasklist_append(struct tasklist *list, const struct task *task);
void tasklist_remove(struct tasklist *list, size_t i);

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
	struct tasklist tasks;
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


struct xss {
	unsigned long idle;
	bool active;
};

void xss_init(void);
void xss_deinit(void);
struct xss xss_query(void);

#endif // IDLEMON_H
