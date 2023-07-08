
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idlemon.h"

#define MAX_ARGS 32

// TODO: Support parsing quoted arguments. Also allocate fields on heap
//       so there's no maximum number.
static char **
parse_argv(char *s)
{
	char *field_save = NULL;
	char *fields[MAX_ARGS + 1];
	size_t fields_len = 0;
	char **argv;

	for (char *field = strtok_r(s, " \t", &field_save); field != NULL;
			field = strtok_r(NULL, " \t", &field_save)) {
		if (fields_len >= MAX_ARGS) {
			log_error("config: too many arguments");
			goto failed;
		}
		if ((fields[fields_len++] = strdup(field)) == NULL) {
			log_error("config: strdup failed:");
			goto failed;
		}
	}

	fields[fields_len++] = NULL;
	if ((argv = malloc(sizeof(*argv) * fields_len)) == NULL) {
		log_error("config: malloc failed:");
		goto failed;
	}
	memcpy(argv, fields, sizeof(*argv) * fields_len);
	return argv;

failed:
	for (size_t i = 0; i < fields_len; i++) {
		if (fields[i] != NULL) {
			free(fields[i]);
		}
	}
	return NULL;
}

static unsigned long
parse_duration(char *s)
{
	bool number = true;
	unsigned long n = 0;
	unsigned long ms = 0;

	for (;;) {
		s = strltrim(s);
		if (*s == '\0') {
			break;
		}

		if (number) {
			char *end = s;

			errno = 0;
			n = strtoul(s, &end, 10);
			if (errno != 0 || end == s) {
				log_error("config: invalid number");
				return ULONG_MAX;
			}

			s = end;
			number = false;
		} else {
			// units
			char c = *s++;
			switch (c) {
			case '\0': ms += n; break;
			case 's':  ms += n * 1000; break;
			case 'm':  ms += n * 1000 * 60; break;
			case 'h':  ms += n * 1000 * 60 * 60; break;
			case 'd':  ms += n * 1000 * 60 * 60 * 24; break;
			case 'w':  ms += n * 1000 * 60 * 60 * 24 * 7; break;
			default:
				log_error("config: invalid unit '%c'", c);
				return ULONG_MAX;

			}
			number = true;
		}
	}

	if (!number) {
		ms += n;
	}

	return ms;
}

static bool
append_task(struct config *cfg, struct task *task)
{
	struct task *t;

	if (task->argv == NULL) {
		// TODO: store section_line_num and log it here
		log_error("config: task.argv required");
		return false;
	}

	if (task->delay == 0) {
		task->delay = cfg->delay;
	}

	if ((t = malloc(sizeof(*t))) == NULL) {
		log_error("config: malloc failed:");
		return false;
	}

	memcpy(t, task, sizeof(*t));
	memset(task, 0, sizeof(*task));

	t->next = cfg->tasks;
	cfg->tasks = t;
	return true;
}

bool
config_load(const char *filename, struct config *cfg)
{
	FILE *f;
	char *line = NULL;
	size_t line_cap = 0;
	size_t line_num = 0;
	enum {
		SECTION_GLOBAL,
		SECTION_LOG,
		SECTION_TASK,
		SECTION_UNKNOWN,
	} section = SECTION_GLOBAL;
	struct task task = {0};

	*cfg = (struct config)CONFIG_INIT;

	if ((f = fopen(filename, "r")) == NULL) {
		log_error("config: failed to open: %s:", filename);
		return NULL;
	}

	for (;;) {
		ssize_t n;
		char *s, *key, *val;
		
		errno = 0;
		if ((n = getline(&line, &line_cap, f)) == -1) {
			if (errno != 0) {
				log_error("config: failed to read:");
				goto failed;
			}
			break;
		}

		line_num++;

		s = strntrim(line, n);
		switch (*s) {
		case '\0':
		case '#':
			continue;
		case '[':
			if (section == SECTION_TASK) {
				if (!append_task(cfg, &task)) {
					goto failed;
				}
			}

			s++;
			strtolower(s);
			if (strcmp(s, "task]") == 0) {
				section = SECTION_TASK;
			} else if (strcmp(s, "log]") == 0) {
				section = SECTION_LOG;
			} else {
				section = SECTION_UNKNOWN;
				log_warn("config: unknown section '%s' on line %zu", s, line_num);
			}
			continue;
		}

		// only process entries for known sections
		if (section == SECTION_UNKNOWN) {
			continue;
		}

		key = s;
		if ((s = strchr(s, '=')) == NULL) {
			log_warn("config: missing '=' on line %zu", line_num);
			continue;
		}
		key = strtolower(strrntrim(key, s - key));
		*s++ = '\0';
		val = strltrim(s);

		if (*val == '\0') {
			log_error("config: empty value for key '%s' on line %zu", key, line_num);
			goto failed;
		}

		switch (section) {
		case SECTION_GLOBAL:
			if (strcmp(key, "delay") == 0) {
				if ((cfg->delay = parse_duration(val)) == ULONG_MAX) {
					log_error("config: invalid delay duration on line %zu", line_num);
					goto failed;
				}
				continue;
			}
			break;

		case SECTION_LOG:
			if (strcmp(key, "level") == 0) {
				strtolower(val);
				if (strcmp(val, "error") == 0) {
					cfg->log.level = LOG_ERROR;
				} else if (strcmp(val, "warn") == 0) {
					cfg->log.level = LOG_WARN;
				} else if (strcmp(val, "info") == 0) {
					cfg->log.level = LOG_INFO;
				} else if (strcmp(val, "debug") == 0) {
					cfg->log.level = LOG_DEBUG;
				} else {
					log_error("config: invalid value for log.level on line %zu",
							line_num);
					goto failed;
				}
				continue;
			} else if (strcmp(key, "time") == 0) {
				strtolower(val);
				switch (strtobool(val)) {
				case 0: cfg->log.time = false; break;
				case 1: cfg->log.time = true;  break;
				default:
					log_error("config: invalid boolean value for log.time on line %zu",
							line_num);
					goto failed;
				}
				continue;
			}
			break;

		case SECTION_TASK:
			if (strcmp(key, "argv") == 0) {
				if (task.argv != NULL) {
					goto duplicate_key;
				}
				if ((task.argv = parse_argv(val)) == NULL) {
					log_error("config: failed to parse task.argv on line %zu", line_num);
					goto failed;
				}
				continue;
			} else if (strcmp(key, "delay") == 0) {
				if (task.delay != 0) {
					goto duplicate_key;
				}
				if ((task.delay = parse_duration(val)) == ULONG_MAX) {
					log_error("config: invalid task.delay duration on line %zu", line_num);
					goto failed;
				}
				continue;
			}
			break;

		case SECTION_UNKNOWN:
			break;
		}

		log_warn("config: unknown key '%s' on line %zu", key, line_num);
		continue;

duplicate_key:
		log_error("config: multiple '%s' keys in section on line %zu",
				key, line_num);
		goto failed;
	}

	if (section == SECTION_TASK) {
		if (!append_task(cfg, &task)) {
			goto failed;
		}
	}

	goto cleanup;

failed:
	config_deinit(cfg);
	task_deinit(&task);

	memset(cfg, 0, sizeof(*cfg));

cleanup:
	if (line != NULL) {
		free(line);
	}
	fclose(f);
	return cfg->tasks != NULL;
}

bool
config_load_and_swap(const char *filename)
{
	struct config cfg;

	if (!config_load(filename, &cfg)) {
		return false;
	}

	config_deinit(&config);
	memcpy(&config, &cfg, sizeof(config));

	log_info("config: loaded %s", filename);
	return true;
}

void
config_deinit(struct config *cfg)
{
	for (struct task *task = cfg->tasks, *next = NULL; task != NULL; task = next) {
		next = task->next;
		task_destroy(task);
	}
}

