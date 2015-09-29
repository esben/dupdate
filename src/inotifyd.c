/*
 * Copyright 2010-2013 Prevas A/S.
 * Copyright 2015 DEIF A/S.
 *
 * This file is part of dupdate.
 *
 * dupdate is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * dupdate is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with dupdate.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <linux/limits.h>

#include "common.h"
#include "daemon.h"

/*
 * This is a simple inotify daemon, compatible with inotifyd applet in
 * busybox.  If you are using busybox, there is no reason to use the inotifyd
 * from dupdate.
 *
 * When used with dupdate-inotifyd-agent, it should be a full replacement for
 * the old dupdate daemon.
 */

#define DEFAULT_PIDFILE			"/var/run/inotifyd.pid"

struct inotifyd_watch_arg {
	char * path;
	uint32_t mask;
};

struct inotifyd_args {
	char * prog;
	int flags;
#ifdef HAVE_FORK
	char * pidfile;
#endif /* HAVE_FORK */
	unsigned num_watches;
	struct inotifyd_watch_arg *watch;
};

#define INOTIFYD_FLAG_DETACH		(1 << 0)
#define INOTIFYD_FLAG_SYSLOG		(1 << 1)

static char *shcmd;
static int shcmd_len;
static const char *cwd;

struct inotify_event_type {
	char ch;
	uint32_t bm;
};

static struct inotify_event_type maskable_events[] = {
	{ 'a', IN_ACCESS },
	{ 'c', IN_MODIFY },
	{ 'e', IN_ATTRIB },
	{ 'w', IN_CLOSE_WRITE },
	{ '0', IN_CLOSE_NOWRITE },
	{ 'r', IN_OPEN },
	{ 'm', IN_MOVED_FROM },
	{ 'y', IN_MOVED_TO },
	{ 'n', IN_CREATE },
	{ 'd', IN_DELETE },
	{ 'D', IN_DELETE_SELF },
	{ 'M', IN_MOVE_SELF },
	{ '\0', 0 }
};

static struct inotify_event_type unmaskable_events[] = {
	{ 'u', IN_UNMOUNT },
	{ 'o', IN_Q_OVERFLOW },
	{ 'x', IN_IGNORED },
	{ '\0', 0 }
};

struct inotifyd_state {
	int fd;
	char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
	ssize_t buf_len;
	int wd[];
};

static struct inotify_event * get_inotify_event(struct inotifyd_state *state)
{
	struct inotify_event *event;
	ssize_t len, event_len;

	/* Read at least one event into buffer */
	event = (struct inotify_event *)state->buf;
	while (state->buf_len < sizeof(*event) &&
	       state->buf_len < (sizeof(*event) + event->len)) {
		len = read(state->fd, state->buf + state->buf_len,
			   sizeof(state->buf) - state->buf_len);
		if (len == -1) {
			PERROR("read", errno);
			return NULL;
		}
		state->buf_len += len;
	}

	/* Copy event to new buffer */
	event_len = sizeof(*event) + event->len;
	event = malloc(event_len);
	if (!event) {
		ERROR("out of memory");
		return NULL;
	}
	memcpy(event, state->buf, event_len);

	/* Ready buffer for next event */
	if (state->buf_len > event_len) {
		state->buf_len -= event_len;
		memmove(state->buf, state->buf + event_len, state->buf_len);
	} else {
		state->buf_len = 0;
	}

	return event;
}

static struct inotifyd_state * inotifyd_init(struct inotifyd_args *args)
{
	struct inotifyd_state *state;
	int i;

	state = malloc(sizeof(*state) +
		       sizeof(*state->wd) * args->num_watches);
	if (!state) {
		ERROR("out of memory");
		exit(EXIT_FAILURE);
	}

	/* Initialize inotify instance */
	state->fd = inotify_init();
	if (state->fd == -1) {
		PERROR("inotify_init", errno);
		exit(EXIT_FAILURE);
	}

	/* Add watches */
	for (i = 0 ; i < args->num_watches ; i++) {
		state->wd[i] = inotify_add_watch(
			state->fd, args->watch[i].path, args->watch[i].mask);
		if (state->wd[i] == -1) {
			PERROR("inotify_add_watch", errno);
			exit(EXIT_FAILURE);
		}
	}

	state->buf_len = 0;

	return state;
}

static int inotifyd_event_loop(struct inotifyd_args *args,
			       struct inotifyd_state *state)
{
	struct inotify_event *event;
	char events_buf[sizeof(maskable_events)/sizeof(*maskable_events) +
			sizeof(unmaskable_events)/sizeof(*unmaskable_events)];
	char *events, *watch;
	struct inotify_event_type *event_type;
	int err, i;

	while ((event = get_inotify_event(state)) != NULL) {

		/* Create string representing the event type */
		events = events_buf;
		event_type = maskable_events;
		while (event_type->bm) {
			if (event->mask & event_type->bm)
				*(events++) = event_type->ch;
			event_type++;
		}
		event_type = unmaskable_events;
		while (event_type->bm) {
			if (event->mask & event_type->bm)
				*(events++) = event_type->ch;
			event_type++;
		}
		*events = '\0';
		events = events_buf;

		/* Get name of watch */
		watch = "<unknown>";
		for (i = 0 ; i < args->num_watches ; i++)
			if (event->wd == state->wd[i])
				watch = args->watch[i].path;

		if (event->len)
			snprintf(shcmd, shcmd_len, "\"%s\" %s \"%s\" \"%s\"",
				 args->prog, events, watch, event->name);
		else
			snprintf(shcmd, shcmd_len, "\"%s\" %s \"%s\"",
				 args->prog, events, watch);
		if ((err = run_shcmd(shcmd))) {
			PERROR(args->prog, err);
			continue;
		}

		free(event);
	}

	printf("leaving event loop...\n");

	return 0;
}

static const char *usage = "\
Usage: %s [options] <PROG> <FILE1>[:MASK]...\n\n\
Arguments:\n\
  <PROG>                Program to run on each event\n\
  <FILEn>               File or directory to watch\n\
  <MASK>                List of events to wait for\n\n\
Options:\n\
  -l, --syslog          Output syslog instead of stdout/stderr\n\
"
#ifdef HAVE_FORK
"\
  -d, --detach          Run in background and detach from controlling process\n\
  -p, --pidfile=<path>  File to write daemon PID to\n\
"
#endif /* HAVE_FORK */
"\n\
Events:\n\
  a   File was accessed\n\
  c   File was modified\n\
  e   Metadata changed\n\
  w   File opened for writing was closed\n\
  0   File or directory not opened for writing was closed\n\
  r   File or directory was opened\n\
  D   Watched file or directory was itself deleted\n\
  M   Watched file or directory was itself moved\n\
  u   Filesystem containing watched object was unmounted\n\
  o   Event queue overflowed\n\
  x   Watch was removed explicitly or automatically\n\
If watching a directory:\n\
  m   File was moved out of watched directory\n\
  y   File was moved into watched directory\n\
  n   File or directory created in watched directory\n\
  d   File or directory deleted from watched directory\n\n\
PROG must not block, as inotifyd wait for it to exit.\n\
When 'x' event is received for all watches, inotifyd exits.\n\
";

static struct option longopts[] = {
	{"syslog",	no_argument,		0, 'l'},
#ifdef HAVE_FORK
	{"detach",	no_argument,		0, 'd'},
	{"pidfile",	required_argument,	0, 'p'},
#endif /* HAVE_FORK */
	{"help",	no_argument,		0, 'h'},
	{0, 0, 0, 0}
};
static const char *optstring = "l"
#ifdef HAVE_FORK
	"dp:"
#endif /* HAVE_FORK */
	"h";

struct inotifyd_args * parse_args(int argc, char *argv[])
{
	int opt, longindex, err;
	struct inotifyd_args *args;
	int i;
	struct inotify_event_type *event_type;
	char *mask_arg;

	args = malloc(sizeof(*args));
	if (args == NULL) {
		ERROR("out of memory");
		exit(EXIT_FAILURE);
	}
	memset(args, 0, sizeof(*args));

	/* Parse argument using getopt_long */
	while ((opt = getopt_long(argc, argv, optstring, longopts,
				  &longindex)) != -1) {
		int err = 0;

		switch (opt) {

		case 'l':
			args->flags |= INOTIFYD_FLAG_SYSLOG;
			break;

#ifdef HAVE_FORK
		case 'p':
			err = strset(&args->pidfile, optarg, PATH_MAX);
			if (err) {
				PERROR("strset", err);
			}
			break;

		case 'd':
			args->flags |= INOTIFYD_FLAG_DETACH;
			break;
#endif /* HAVE_FORK */

		case 'h':
			printf(usage, argv[0]);
			exit(EXIT_SUCCESS);
			break;

		case '?':
			exit(EXIT_FAILURE);
			break;

		default:
			ERROR("invalid option -%c", opt);
			exit(EXIT_FAILURE);
		}

		if (err) {
			if (longindex) {
				ERROR("error while processing option --%s",
				      longopts[longindex].name);
			} else {
				ERROR("error while processing option -%c", opt);
			}
			exit(EXIT_FAILURE);
		}
	}

	if ((argc - optind) < 2) {
		ERROR("not enough arguments %d %d", argc, optind);
		exit(EXIT_FAILURE);
	}

	err = strset(&args->prog, argv[optind], PATH_MAX);
	if (err) {
		PERROR("strset", err);
		exit(EXIT_FAILURE);
	}

	args->num_watches = argc - optind - 1;
	args->watch = malloc(sizeof(*args->watch) * args->num_watches);
	if (args->watch == NULL) {
		ERROR("out of memory");
		exit(EXIT_FAILURE);
	}
	memset(args->watch, 0, sizeof(*args->watch) * args->num_watches);
	for (i = 0 ; i < (argc - optind - 1) ; i++) {
		args->watch[i].path = argv[optind + 1 + i];
		mask_arg = strchr(args->watch[i].path, ':');
		if (mask_arg) {
			args->watch[i].mask = 0;
			*mask_arg = '\0';
			mask_arg++;
			while (*mask_arg != '\0') {
				event_type = maskable_events;
				while (event_type->ch != '\0') {
					if (*mask_arg == event_type->ch) {
						args->watch[i].mask |=
							event_type->bm;
						break;
					}
					event_type++;
				}
				mask_arg++;
			}
		} else {
			args->watch[i].mask = IN_ALL_EVENTS;
		}
	}

	if (args->flags & INOTIFYD_FLAG_DETACH && !args->pidfile)
		args->pidfile = DEFAULT_PIDFILE;

	return args;
}

static int get_sysconf(void)
{
	shcmd_len = sysconf(_SC_ARG_MAX);
	if (shcmd_len == -1) {
		PERROR("sysconf", errno);
		return errno;
	}

	shcmd = malloc(shcmd_len);
	if (!shcmd) {
		PERROR("malloc", ENOMEM);
		return ENOMEM;
	}

	cwd = get_current_dir_name();

	return 0;
}

int main (int argc, char *argv[])
{
	struct inotifyd_args *args;
	struct inotifyd_state *state;

	args = parse_args(argc, argv);

	if (args->flags & INOTIFYD_FLAG_SYSLOG)
		log_to_syslog(1);

	if (get_sysconf())
		exit(EXIT_FAILURE);

	state = inotifyd_init(args);
	if (!state)
		exit(EXIT_FAILURE);

	if (args->flags & INOTIFYD_FLAG_DETACH)
		daemonize(args->pidfile);

	return inotifyd_event_loop(args, state);
}
