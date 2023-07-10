
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "idlemon.h"


static void
task_start(struct task *task)
{
	pid_t pid;

	if ((pid = fork()) == -1) {
		log_fatal("task: [%s] fork failed:", task->name);
		return;
	} else if (pid > 0) {
		log_info("task: [%s] started", task->name);
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
		log_error("task: [%s] waitpid failed:", task->name);
		task->state = TASK_COMPLETED;
		return true;
	case 0:
		return false;
	}

	if (WIFEXITED(status)) {
		int code = WEXITSTATUS(status);
		switch (code) {
		case 255:
			log_error("task: [%s] failed to start", task->name);
			break;
		case 254:
			log_error("task: [%s] not found", task->name);
			break;
		default:
			if (code != 0) {
				log_error("task: [%s] exited with non-zero status (%d)",
						task->name, code);
			}
			break;
		}
		task->state = TASK_COMPLETED;
		return true;
	}

	if (WIFSIGNALED(status)) {
		int sig = WTERMSIG(status);
		log_warn("task: [%s] received signal (%d)", task->name, sig);
		task->state = TASK_COMPLETED;
		return true;
	}

	return false;
}

static void
task_reset(struct task *task)
{
	log_debug("task: [%s] reset", task->name);
	task->state = TASK_PENDING;
}

void
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
		log_info("task: [%s] complete", task->name);
		// waited upon task has completed so we can run completed branch
		// fallthrough
	case TASK_COMPLETED:
		if (idle_reset) {
			task_reset(task);
		}
	}
}

void
task_deinit(struct task *task)
{
	if (task->name != NULL) {
		free(task->name);
	}
	if (task->argv != NULL) {
		for (char **p = task->argv; *p != NULL; p++) {
			free(*p);
		}
		free(task->argv);
	}
}

void
task_destroy(struct task *task)
{
	task_deinit(task);
	free(task);
}

