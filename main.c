
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// used by `xss_get_idle()`
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

#include "idlemon.h"

bool color_tty = true;
struct log_config log_config = {
	.level = LOG_DEBUG,
	.time = true,
};
static time_t signal_time = 0;

static void
signal_handler(int sig)
{
	if (sig == SIGUSR1) {
		signal_time = time(NULL);
	}
}

static unsigned long
signal_get_idle(void)
{
	time_t now = time(NULL);
	return now > signal_time ? (now - signal_time) * 1000 : 0;
}

static unsigned long
xss_get_idle(void)
{
	static Display *dpy = NULL;
	static XScreenSaverInfo *info = NULL;

	if (info == NULL) {
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

	if (XScreenSaverQueryInfo(dpy, XDefaultRootWindow(dpy), info) == 0) {
		log_fatal("xss: query failed");
	}

	return info->idle;
}

static unsigned long
get_idle(void)
{
	unsigned long signal_idle, xss_idle;

	signal_idle = signal_get_idle();
	xss_idle = xss_get_idle();

	return xss_idle < signal_idle ? xss_idle : signal_idle;
}

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

static void
task_start(struct task *task)
{
	pid_t pid;

	if ((pid = fork()) == -1) {
		log_fatal("fork():");
		return;
	} else if (pid > 0) {
		log_info("task started: %s", task->argv[0]);
		task->pid = pid;
		task->state = TASK_STARTED;
		return;
	}

	execvp(task->argv[0], task->argv);
	_exit(errno == ENOENT ? 254 : 255);
}

static bool
task_wait(struct task *task)
{
	int status = 0;

	switch (waitpid(task->pid, &status, WNOHANG)) {
	case -1:
		log_error("waitpid failed:");
		task->state = TASK_COMPLETED;
		return true;
	case 0:
		return false;
	}

	if (WIFEXITED(status)) {
		int code = WEXITSTATUS(status);
		switch (code) {
		case 255:
			log_error("task failed to start: %s", task->argv[0]);
			break;
		case 254:
			log_error("task failed to start: %s not found", task->argv[0]);
			break;
		default:
			if (code != 0) {
				log_error("task exited with non-zero status (%d): %s", code,
						task->argv[0]);
			}
			break;
		}
		task->state = TASK_COMPLETED;
		return true;
	}

	if (WIFSIGNALED(status)) {
		int sig = WTERMSIG(status);
		log_warn("task received signal (%d): %s", sig, task->argv[0]);
		task->state = TASK_COMPLETED;
		return true;
	}

	return false;
}

static void
task_reset(struct task *task)
{
	log_debug("task reset: %s", task->argv[0]);
	task->state = TASK_PENDING;
}

static void
task_process(struct task *task, unsigned long idle, bool idle_reset)
{
	switch (task->state) {
	case TASK_PENDING:
		if (idle >= task->delay) {
			task_start(task);
		}
		break;
	case TASK_STARTED:
		if (!task_wait(task)) {
			break;
		}
		log_info("task complete: %s", task->argv[0]);
		// waited upon task has completed so we can run completed branch
		// fallthrough
	case TASK_COMPLETED:
		if (idle_reset) {
			task_reset(task);
		}
	}
}

int
main(int argc, char **argv)
{
	struct sigaction sa = {0};
	struct task *tasks = NULL;
	unsigned long prev_idle = 0;

	(void)argc;
	(void)argv;

	color_tty = getenv("NOCOLOR") == NULL;

	tasks = &(struct task){
		.next = tasks,
		.argv = (char *[]){"./file-that-should-not-exist", NULL},
		.delay = 2000,
	};
	tasks = &(struct task){
		.next = tasks,
		.argv = (char *[]){"date", "-Is", NULL},
		.delay = 3000,
	};
	tasks = &(struct task){
		.next = tasks,
		.argv = (char *[]){"sleep", "100", NULL},
		.delay = 3001,
	};

	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		log_fatal("failed to register signal handler:");
	}

	for (;;) {
		unsigned long idle = get_idle();
		bool idle_reset = idle < prev_idle;

		log_debug("loop: idle=%ld, idle_reset=%d", idle, idle_reset);

		for (struct task *task = tasks; task != NULL; task = task->next) {
			task_process(task, idle, idle_reset);
		}

		prev_idle = idle;
		sleep(1);
	}

	return 0;
}

