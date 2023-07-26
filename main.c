
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "idlemon.h"


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

static pid_t
get_active_instance(void)
{
	struct stat st;
	ino_t inode;
	DIR *dir;
	uid_t uid = getuid();

	if (stat("/proc/self/exe", &st) == -1) {
		log_fatal("failed to stat /proc/self/exe:");
	}
	inode = st.st_ino;

	if ((dir = opendir("/proc")) == NULL) {
		log_fatal("failed to open /proc:");
	}

	for (;;) {
		struct dirent *entry;
		char path[64];
		ssize_t r;

		errno = 0;
		if ((entry = readdir(dir)) == NULL) {
			if (errno != 0) {
				log_fatal("failed to read /proc:");
			}
			break;
		}

		if (!isdigit(*entry->d_name)) {
			continue;
		}

		r = snprintf(path, sizeof(path), "/proc/%s", entry->d_name);
		if (r < 0 || (size_t)r >= sizeof(path)) {
			log_fatal("path overflow");
		}
		if (lstat(path, &st) == -1 || !S_ISDIR(st.st_mode) || st.st_uid != uid) {
			continue;
		}

		r = snprintf(path, sizeof(path), "/proc/%s/exe", entry->d_name);
		if (r < 0 || (size_t)r >= sizeof(path)) {
			log_fatal("path overflow");
		}

		if (stat(path, &st) == -1) {
			continue;
		}

		if (st.st_ino == inode) {
			unsigned long n;
			char *end = entry->d_name;
			pid_t pid;

			errno = 0;
			n = strtoul(entry->d_name, &end, 10);
			if (n == 0 || errno != 0 || *end != '\0') {
				log_fatal("invalid pid string '%s'", entry->d_name);
			}
			pid = (pid_t)n;

			if (getpid() != pid) {
				closedir(dir);
				return pid;
			}
		}
	}

	return -1;
}

int
main(int argc, char **argv)
{
	int opt;
	char *config_filename = NULL;
	pid_t active_instance;
	struct state state = {0}, prev_state = {0};

	color_tty = getenv("NO_COLOR") == NULL && isatty(STDERR_FILENO);

	active_instance = get_active_instance();

	while ((opt = getopt(argc, argv, "hprc:")) != -1) {
		switch (opt) {
		case 'c':
			if (config_filename != NULL) {
				free(config_filename);
			}
			if ((config_filename = strdup(optarg)) == NULL) {
				log_fatal("strdup failed:");
			}
			break;

		case 'p':
			if (active_instance == -1) {
				log_fatal("no active instance");
			}
			kill(active_instance, SIGUSR1);
			return 0;

		case 'r':
			if (active_instance == -1) {
				log_fatal("no active instance");
			}
			kill(active_instance, SIGUSR2);
			return 0;

		case 'h':
		default:
			fprintf(stderr,
					"Usage: %s [options]\n"
					"\n"
					"Execute tasks based on the time the system has been idle.\n"
					"\n"
					"Options:\n"
					"  -c <filename> (default: ~/.config/idlemon.conf) config filename\n"
					"  -p            ping active instance\n"
					"  -r            reload config of active instance\n"
					"\n",
					argv[0]);
			exit(1);
		}
	}

	// Only allow a single instance
	if (active_instance != -1) {
		log_fatal("active instance found");
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
		unsigned long signal_idle;
		struct xss xss;

		if (reload_config) {
			config_load_and_swap(config_filename);
			reload_config = false;
		}

		xss = xss_query();
		signal_idle = signal_get_idle();

		state.idle = xss.idle < signal_idle ? xss.idle : signal_idle;
		state.xss_active = xss.active;

		log_debug("loop: idle=%ld, xss_active=%s", state.idle,
				state.xss_active ? "true" : "false");

		for (size_t i = 0; i < config.tasks.len;) {
			struct task *task = &config.tasks.entries[i];
			if (task_process(task, &state, &prev_state)) {
				log_debug("removed temporary task '%s'", task->name);
				tasklist_remove(&config.tasks, i);
			} else {
				i++;
			}
		}

		memcpy(&prev_state, &state, sizeof(prev_state));
		sleep(1);
	}

	config_deinit(&config);
	xss_deinit();
	free(config_filename);

	log_info("finished");

	return 0;
}

