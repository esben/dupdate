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

static const char *usage = \
	"Usage: %s <dir> [options]\n\n"					\
	"Arguments:\n"							\
	"  <dir>                 Directory to watch\n\n"		\
	"Options:\n"							\
	"  -x, --exefile=<file>  Name of file in archive to execute\n"	\
	"  -u, --unpack=<dir>    Directory to unpack archive in\n"	\
	"  -p, --pidfile=<path>  File to write daemon PID to\n"		\
	"  -f, --foreground      Run in the foreground\n"		\
	"  -F, --detach          Run in the foreground but detach the process from the \n" \
	"                        controlling terminal and current directory\n" \
	"  -l, --syslog          Output syslog instead of stdout/stderr\n" \
	"  -R, --no-remove       Don't remove archive after unpack\n"	\
	"  -C, --no-cleanup      Don't remove unpacked files when done\n" \
	"  --help                Display help\n"			\
	;

static struct option longopts[] = {
	{"exefile",	required_argument,	0, 'x'},
	{"unpack",	required_argument,	0, 'u'},
	{"pidfile",	required_argument,	0, 'p'},
	{"foreground",	no_argument,		0, 'f'},
	{"detach",	no_argument,		0, 'F'},
	{"syslog",	no_argument,		0, 'l'},
	{"no-remove",	no_argument,		0, 'R'},
	{"no-cleanup",	no_argument,		0, 'C'},
	{"help",	no_argument,		0, 'h'},
	{0, 0, 0, 0}
};
static const char *optstring = "x:u:p:fFlRCh";

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

		case 'x':
			err = strset(&cfg.exefile, optarg, NAME_MAX);
			if (err)
				PERROR("strset", err);
			break;

		case 'u':
			err = strset(&cfg.unpackdir, optarg, PATH_MAX - 7);
			if (err)
				PERROR("strset", err);
			break;

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

		case 'l':
			cfg.flags |= DUPDATE_FLAG_SYSLOG;
			break;

		case 'R':
			cfg.flags |= DUPDATE_FLAG_NO_REMOVE;
			break;

		case 'C':
			cfg.flags |= DUPDATE_FLAG_NO_CLEANUP;
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

	if (!cfg.exefile)
		cfg.exefile = DEFAULT_EXEFILE;
}
