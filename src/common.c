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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "common.h"

int strset(char **ptr, const char *str, size_t maxlen)
{
	if (*ptr) {
		printf("%s: ->free\n", __func__);
		free(*ptr);
		printf("%s: <-free\n", __func__);
	}

	if (maxlen && strlen(str) >= maxlen)
		return EINVAL;

	*ptr = strdup(str);
	if (!*ptr)
		return ENOMEM;

	return 0;
}

int _log_to_syslog = 0;

void log_to_syslog(int enable)
{
	if (enable)
		_log_to_syslog = 1;
}

int run_shcmd(const char *cmd)
{
	int ret;

	INFO("+ %s", cmd);
	ret = system(cmd);

	if (WIFSIGNALED(ret) && (WTERMSIG(ret) == SIGINT)) {
		INFO("<SIGINT>\n");
		exit(EINTR);
	}
	if (WIFSIGNALED(ret) && (WTERMSIG(ret) == SIGQUIT)) {
		INFO("<SIGQUIT>\n");
		exit(EINTR);
	}
	if (WIFSIGNALED(ret)) {
		ERROR("%s terminated by signal %d", cmd, WTERMSIG(ret));
		return EINTR;
	}

	if (ret == -1)
		return ECHILD;

	if (WEXITSTATUS(ret) == 127)
		return ENOENT;

	return WEXITSTATUS(ret);
}
