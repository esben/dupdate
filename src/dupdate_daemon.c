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

#include "dupdate.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


static void
write_pidfile(const char *path)
{
	int pid_fd;
	char buf[8];
	int n;

	if (!path)
		return;

	pid_fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (pid_fd < 0)
		return;

	n = snprintf(buf, 8, "%d\n", getpid());

	if (write(pid_fd, buf, n) == -1)
		PERROR("write", errno);

	close(pid_fd);
}

void
daemonize(void)
{
	pid_t pid, sid;

	/* Return if already a daemon */
	if (getppid() == 1)
		return;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		PERROR("fork", errno);
		exit(EXIT_FAILURE);
	}

	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* At this point we are executing as the child process */

	write_pidfile(cfg.pidfile);

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		PERROR("setsid", errno);
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory.  This prevents the current
	directory from being locked; hence not being able to remove it. */
	if ((chdir("/")) < 0) {
		PERROR("chdir", errno);
		exit(EXIT_FAILURE);
	}

	/* Redirect standard files to /dev/null */
	if (freopen( "/dev/null", "r", stdin) == NULL)
		PERROR("freopen: stdin", errno);
	if (freopen( "/dev/null", "w", stdout) == NULL)
		PERROR("freopen: stdout", errno);
	if (freopen( "/dev/null", "w", stderr) == NULL)
		PERROR("freopen: stderr", errno);
}
