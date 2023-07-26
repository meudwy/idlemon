
#include <errno.h>
#include <stdlib.h>
#include <string.h>
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

bool
task_process(struct task *task, const struct state *state,
		const struct state *prev_state)
{
	switch (task->state) {
	case TASK_PENDING:
		{
			bool start = task->delay == TASK_DELAY_XSS
				? state->xss_active
				: state->idle >= task->delay;

			if (start) {
				task_start(task);
			}
			break;
		}
	case TASK_STARTED:
		if (!task_wait(task)) {
			break;
		}
		log_info("task: [%s] complete", task->name);
		// waited upon task has completed so we can run completed branch
		// fallthrough
	case TASK_COMPLETED:
		if (task->temporary) {
			return true;
		} else {
			bool reset = task->delay == TASK_DELAY_XSS
				? state->xss_active != prev_state->xss_active
				: state->idle < prev_state->idle;

			if (reset) {
				task_reset(task);
			}
		}
	}
	return false;
}

struct task *
task_clone(struct task *dst, const struct task *src)
{
	size_t argv_len;
	char **pp;

	memset(dst, 0, sizeof(*dst));

	if ((dst->name = strdup(src->name)) == NULL) {
		return NULL;
	}

	for (pp = src->argv, argv_len = 0; *pp != NULL; pp++, argv_len++) {
	}
	
	if ((dst->argv = calloc(argv_len + 1, sizeof(*dst->argv))) == NULL) {
		goto failed;
	}

	for (size_t i = 0; i < argv_len; i++) {
		if ((dst->argv[i] = strdup(src->argv[i])) == NULL) {
			goto failed;
		}
	}

	dst->delay = src->delay;
	dst->pid = src->pid;
	dst->state = src->state;
	dst->temporary = src->temporary;

	return dst;

failed:
	task_deinit(dst);
	return NULL;
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

bool
tasklist_append(struct tasklist *list, const struct task *task)
{
	if (list->len >= list->cap) {
		size_t cap = list->cap == 0 ? 8 : list->cap * 2;
		struct task *entries = realloc(list->entries, cap * sizeof(*entries));
		if (entries == NULL) {
			return false;
		}
		list->cap = cap;
		list->entries = entries;
	}

	memcpy(&list->entries[list->len++], task, sizeof(*task));
	return true;
}

void
tasklist_remove(struct tasklist *list, size_t i)
{
	if (i >= list->len) {
		return;
	}

	task_deinit(&list->entries[i]);

	if (i != list->len - 1) {
		// Swap with last entry as we don't care about maintaining order
		memcpy(&list->entries[i], &list->entries[list->len - 1],
				sizeof(*list->entries));
	}

	list->len--;
}

