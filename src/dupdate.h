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
#include <syslog.h>
#include <string.h>

#include "config.h"

struct dupdate_config {
	char *watchdir;		/* Directory to watch for incoming archives */
	char *unpackdir;	/* Directory to unpack archvies to */
	char *tarcmd;		/* Command to execute in tar archives */
	char *zipcmd;		/* Command to execute in zip archives*/
	char *pidfile;		/* PID file (daemon mode only) */
	int flags;		/* Configuration flags */
};

#define DUPDATE_FLAG_FOREGROUND		(1 << 0)
#define DUPDATE_FLAG_DETACH		(1 << 1)
#define DUPDATE_FLAG_SYSLOG		(1 << 2)
#define DUPDATE_FLAG_NO_REMOVE		(1 << 3)
#define DUPDATE_FLAG_NO_CLEANUP		(1 << 4)

#define DEFAULT_PIDFILE			"/var/run/dupdate.pid"
#define DEFAULT_CMDFILE			"run"

extern struct dupdate_config cfg;

#define INFO(format, args...)						\
	if (cfg.flags & DUPDATE_FLAG_SYSLOG)				\
		syslog(LOG_INFO, "" format "\n", ## args);		\
	else								\
		printf("" format "\n", ## args)

#define ERROR(format, args...)						\
	if (cfg.flags & DUPDATE_FLAG_SYSLOG)				\
		syslog(LOG_ERR, "%s: " format "\n", __func__,		\
		       ## args);					\
	else								\
		fprintf(stderr, "%s: " format "\n", __func__,		\
			## args)

#define PERROR(str, errnum)						\
	if (cfg.flags & DUPDATE_FLAG_SYSLOG)				\
		syslog(LOG_ERR, "%s: %s: %s\n", __func__, str,		\
		       strerror(errnum));				\
	else								\
		fprintf(stderr, "%s: %s: %s\n", __func__, str,		\
			strerror(errnum))

void parse_args(int argc, char *argv[]);
void daemonize(void);
