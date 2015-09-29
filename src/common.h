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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <syslog.h>
#include <string.h>

#include "config.h"

int strset(char **ptr, const char *str, size_t maxlen);

extern int _log_to_syslog;
void log_to_syslog(int);

#define INFO(format, args...)						\
	if (_log_to_syslog)						\
		syslog(LOG_INFO, "" format "\n", ## args);		\
	else								\
		printf("" format "\n", ## args);

#define ERROR(format, args...)						\
	if (_log_to_syslog)						\
		syslog(LOG_ERR, "%s: " format "\n", __func__,		\
		       ## args);					\
	else								\
		fprintf(stderr, "%s: " format "\n", __func__,		\
			## args);

#define PERROR(str, errnum)						\
	if (_log_to_syslog)						\
		syslog(LOG_ERR, "%s: %s: %s\n", __func__, str,		\
		       strerror(errnum));				\
	else								\
		fprintf(stderr, "%s: %s: %s\n", __func__, str,		\
			strerror(errnum));

int run_shcmd(const char *cmd);

#endif /* _COMMON_H_ */
