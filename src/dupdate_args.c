/*
 * Copyright 2010-2013 Prevas A/S.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <linux/limits.h>

#include "dupdate.h"

#ifdef HAVE_FORK
#define HAVE_FORK_USAGE \
	"  -p, --pidfile=<path>  File to write daemon PID to\n"		\
	"  -f, --foreground      Run in the foreground\n"		\
	"  -F, --detach          Run in the foreground but detach the process from the \n" \
	"                        controlling terminal and current directory\n"
#else /* HAVE_FORK */
#define HAVE_FORK_USAGE ""
#endif /* HAVE_FORK */

static const char *usage = \
	"Usage: %s <dir> [options]\n\n"					\
	"Arguments:\n"							\
	"  <dir>                 Directory to watch\n\n"		\
	"Options:\n"							\
	"  -u, --unpack=<dir>    Directory to unpack archive in\n"	\
	"  -x, --tarcmd=<file>   Name of file in tar archives to execute [default: run]\n" \
	"  -z, --zipcmd=<file>   Name of file in zip archives to execute [default: run]\n" \
	HAVE_FORK_USAGE							\
	"  -l, --syslog          Output syslog instead of stdout/stderr\n" \
	"  -R, --no-remove       Don't remove archive after unpack\n"	\
	"  -C, --no-cleanup      Don't remove unpacked files when done\n" \
	"  -c, --completion      Create completion file when done\n"	\
	"  --help                Display help\n"			\
	;

static struct option longopts[] = {
	{"unpack",	required_argument,	0, 'u'},
	{"tarcmd",	required_argument,	0, 'x'},
	{"zipcmd",	required_argument,	0, 'z'},
	{"pidfile",	required_argument,	0, 'p'},
	{"foreground",	no_argument,		0, 'f'},
	{"detach",	no_argument,		0, 'F'},
	{"syslog",	no_argument,		0, 'l'},
	{"no-remove",	no_argument,		0, 'R'},
	{"no-cleanup",	no_argument,		0, 'C'},
	{"completion",	no_argument,		0, 'c'},
	{"help",	no_argument,		0, 'h'},
	{0, 0, 0, 0}
};
static const char *optstring = "u:x:z:p:fFlRCch";

static void
printf_usage(char *cmd)
{
	printf(usage, cmd);
}

static int
strset(char **ptr, const char *str, size_t maxlen)
{
	size_t len;

	if (*ptr)
		free(*ptr);

	if (maxlen && len >= maxlen)
		return EINVAL;

	*ptr = strdup(str);
	if (!*ptr)
		return ENOMEM;

	return 0;
}

void
parse_args(int argc, char *argv[])
{
	int opt, longindex, err;

	/* Parse argument using getopt_long */
	while ((opt = getopt_long(argc, argv, optstring, longopts,
				  &longindex)) != -1) {
		int err = 0;

		switch (opt) {

		case 'u':
			err = strset(&cfg.unpackdir, optarg, PATH_MAX - 7);
			if (err)
				PERROR("strset", err);
			break;

		case 'x':
			err = strset(&cfg.tarcmd, optarg, NAME_MAX);
			if (err)
				PERROR("strset", err);
			break;

		case 'z':
			err = strset(&cfg.zipcmd, optarg, NAME_MAX);
			if (err)
				PERROR("strset", err);
			break;

#ifdef HAVE_FORK
		case 'p':
			err = strset(&cfg.pidfile, optarg, PATH_MAX);
			if (err)
				PERROR("strset", err);
			break;

		case 'f':
			cfg.flags |= DUPDATE_FLAG_FOREGROUND;
			break;

		case 'F':
			cfg.flags |= (DUPDATE_FLAG_FOREGROUND |
					  DUPDATE_FLAG_DETACH);
			break;
#endif /* HAVE_FORK */

		case 'l':
			cfg.flags |= DUPDATE_FLAG_SYSLOG;
			break;

		case 'R':
			cfg.flags |= DUPDATE_FLAG_NO_REMOVE;
			break;

		case 'C':
			cfg.flags |= DUPDATE_FLAG_NO_CLEANUP;
			break;

		case 'c':
			cfg.flags |= DUPDATE_FLAG_COMPLETION;
			break;

		case 'h':
			printf_usage(argv[0]);
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
			if (longindex)
				ERROR("error while processing option --%s",
				      longopts[longindex].name);
			else
				ERROR("error while processing option -%c", opt);
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		ERROR("dir argument missing");
		exit(EXIT_FAILURE);
	} else if ((argc - optind) > 1) {
		ERROR("too many arguments");
		exit(EXIT_FAILURE);
	}

	err = strset(&cfg.watchdir, argv[optind], PATH_MAX);
	if (err) {
		PERROR("strset", err);
		exit(EXIT_FAILURE);
	}

	if (!cfg.unpackdir)
		cfg.unpackdir = cfg.watchdir;

	if (!cfg.pidfile)
		cfg.pidfile = DEFAULT_PIDFILE;
	if (!cfg.tarcmd)
		cfg.tarcmd = DEFAULT_CMDFILE;
	if (!cfg.zipcmd)
		cfg.zipcmd = DEFAULT_CMDFILE;
}
