
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "idlemon.h"

#ifndef PATH_MAX
#	define PATH_MAX 4096
#endif


bool color_tty = true;
struct config config = CONFIG_INIT;

static bool running = true;
static bool reload_config = false;
static time_t signal_time = 0;


static void
signal_handler(int sig)
{
	switch (sig) {
	case SIGUSR1:
		signal_time = time(NULL);
		break;
	case SIGUSR2:
		reload_config = true;
		break;
	case SIGINT:
		running = false;
		break;
	}
}

static bool
register_signal_handlers(void)
{
	struct sigaction sa = {
		.sa_handler = signal_handler,
		.sa_flags = SA_RESTART,
	};

	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		return false;
	}
	if (sigaction(SIGUSR2, &sa, NULL) == -1) {
		return false;
	}
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		return false;
	}
	return true;
}

static unsigned long
signal_get_idle(void)
{
	time_t now = time(NULL);
	return now > signal_time ? (now - signal_time) * 1000 : 0;
}

static unsigned long
get_idle(void)
{
	unsigned long signal_idle, xss_idle;

	signal_idle = signal_get_idle();
	xss_idle = xss_get_idle();

	return xss_idle < signal_idle ? xss_idle : signal_idle;
}

static char *
xdg_config_filename(void)
{
	char path[PATH_MAX];
	char *env, *s;
	int r;

	if ((env = getenv("XDG_CONFIG_HOME")) != NULL) {
		r = snprintf(path, sizeof(path), "%s/idlemon.conf", env);
	} else {
		struct passwd *pw = getpwuid(getuid());
		if (pw == NULL) {
			log_fatal("failed to get password file entry for user:");
		}
		r = snprintf(path, sizeof(path), "%s/.config/idlemon.conf", pw->pw_dir);
	}

	if (r < 0 || (size_t)r >= sizeof(path)) {
		log_fatal("path overflow");
	}

	if ((s = strdup(path)) == NULL) {
		log_fatal("strdup failed:");
	}
	return s;
}

int
main(int argc, char **argv)
{
	unsigned long prev_idle = 0;
	int opt;
	char *config_filename = NULL;

	color_tty = getenv("NO_COLOR") == NULL && isatty(STDERR_FILENO);

	while ((opt = getopt(argc, argv, "c:")) != -1) {
		if (opt == 'c') {
			if (config_filename != NULL) {
				free(config_filename);
			}
			if ((config_filename = strdup(optarg)) == NULL) {
				log_fatal("strdup failed:");
			}
		} else {
			fprintf(stderr,
					"Usage: %s [options]\n"
					"\n"
					"Execute tasks based on the time the system has been idle.\n"
					"\n"
					"Options:\n"
					"  -c <filename> (default: ~/.config/idlemon.conf) config filename\n"
					"\n",
					argv[0]);
			exit(1);
		}
	}

	if (config_filename == NULL) {
		config_filename = xdg_config_filename();
	}

	if (!config_load_and_swap(config_filename)) {
		exit(1);
	}

	xss_init();

	if (!register_signal_handlers()) {
		log_fatal("failed to register signal handlers:");
	}

	while (running) {
		unsigned long idle;
		bool idle_reset;

		if (reload_config) {
			config_load_and_swap(config_filename);
			reload_config = false;
		}

		idle = get_idle();
		idle_reset = idle < prev_idle;

		log_debug("loop: idle=%ld, idle_reset=%d", idle, idle_reset);

		for (struct task *task = config.tasks, *prev_task = NULL, *next_task;
				task != NULL; task = next_task) {
			next_task = task->next;
			if (task_process(task, idle, idle_reset)) {
				if (prev_task != NULL) {
					prev_task->next = next_task;
				} else {
					config.tasks = next_task;
				}
				log_debug("removed temporary task '%s'", task->name);
				task_destroy(task);
			} else {
				prev_task = task;
			}
		}

		prev_idle = idle;
		sleep(1);
	}

	config_deinit(&config);
	xss_deinit();
	free(config_filename);

	log_info("finished");

	return 0;
}

