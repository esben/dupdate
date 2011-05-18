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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/param.h>

struct dupdate_config cfg;

static char *tmpdir_template;
static char *tmpdir;
static char *shcmd;
static int shcmd_len;

static int
get_inotify_event(int fd, struct inotify_event *event)
{
	ssize_t len;

	/* Read inotify_event header */
	len = read(fd, event, sizeof(*event) + PATH_MAX);
	if (len == -1) {
		PERROR("read", errno);
		return errno;
	}
	if (len < sizeof(*event)) {
		ERROR("partial inotify_event header read: %zu", len);
		return EIO;
	}
	if (len < (sizeof(*event) + event->len)) {
		ERROR("partial inotify_event read: %zu (expected %lu)",
		      len, sizeof(*event) + event->len);
		return EIO;
	}
	if (len > (sizeof(*event) + event->len)) {
		ERROR("excess inotify_event read: %zu (expected %lu)",
		      len, sizeof(*event) + event->len);
		return EIO;
	}

	return 0;
}

static int
remove_archive(const char *path)
{
	int err;

	if (cfg.flags & DUPDATE_FLAG_NO_REMOVE)
		return;

	err = chdir(cfg.watchdir);
	if (err == -1) {
		PERROR("chdir", errno);
		return errno;
	}

	err = unlink(path);
	if (err == -1) {
		PERROR("unlink", errno);
		return errno;
	}

	return 0;
}

static int
cleanup_tmpdir(const char *path)
{
	int err;

	if (cfg.flags & DUPDATE_FLAG_NO_CLEANUP)
		return;

	err = chdir(cfg.watchdir);
	if (err == -1) {
		PERROR("chdir", errno);
		return errno;
	}

	snprintf(shcmd, shcmd_len, "rm -rf %s", path);
	INFO("running: %s", shcmd);
	err = system(shcmd);
	if (err == -1) {
		PERROR(shcmd, errno);
		return errno;
	}

	return 0;
}

static void
handle_inotify_event(struct inotify_event *event)
{
	int err;
	char *exefile;

	if (event->len == 0) {
		ERROR("ignoring anonymous event");
		return;
	}

	strcpy(tmpdir, tmpdir_template);
	if (!mkdtemp(tmpdir)) {
		PERROR("mkdtemp", errno);
		return;
	}

	exefile = malloc(strnlen(tmpdir, PATH_MAX) + 2 +
			 strnlen(cfg.exefile, NAME_MAX));
	if (!exefile) {
		PERROR("malloc", ENOMEM);
		cleanup_tmpdir(tmpdir);
		return;
	}
	strcpy(exefile, tmpdir);
	strcat(exefile, "/");
	strcat(exefile, cfg.exefile);

	INFO("using tmpdir %s", tmpdir);
	err = chdir(tmpdir);
	if (err == -1) {
		PERROR("chdir", errno);
		cleanup_tmpdir(tmpdir);
		return;
	}

	snprintf(shcmd, shcmd_len, "tar -xf %s/%s", cfg.watchdir, event->name);
	INFO("running: %s", shcmd);
	err = system(shcmd);
	if (err == -1) {
		PERROR(shcmd, errno);
		cleanup_tmpdir(tmpdir);
		remove_archive(event->name);
		return;
	} else if (err) {
		PERROR(shcmd, WEXITSTATUS(err));
		cleanup_tmpdir(tmpdir);
		remove_archive(event->name);
		return;
	}

	INFO("running: %s", exefile);
	err = system(exefile);
	if (err == -1) {
		PERROR(shcmd, WEXITSTATUS(err));
		cleanup_tmpdir(tmpdir);
		return;
	}

	remove_archive(event->name);

	INFO("%s/%s done", cfg.watchdir, event->name);

	cleanup_tmpdir(tmpdir);
}

static int
inotify_watch(void)
{
	int fd, wd, err;
	void *event_buf;
	struct inotify_event *event;

	shcmd_len = sysconf(_SC_ARG_MAX);
	if (shcmd_len == -1) {
		PERROR("sysconf", errno);
		return errno;
	}

	tmpdir_template = malloc(strlen(cfg.unpackdir) + 8);
	tmpdir = malloc(strnlen(cfg.unpackdir, PATH_MAX));
	shcmd = malloc(shcmd_len);
	if (!tmpdir_template || !tmpdir || !shcmd) {
		PERROR("malloc", ENOMEM);
		return ENOMEM;
	}
	strcpy(tmpdir_template, cfg.unpackdir);
	strncat(tmpdir_template, "/XXXXXX", PATH_MAX);

	/* Initialize inotify instance */
	fd = inotify_init();
	if (fd == -1) {
		PERROR("inotify_init", errno);
		return errno;
	}

	/* Add watch on directory - only watch create events */
	wd = inotify_add_watch(fd, cfg.watchdir,
			       IN_CLOSE_WRITE | IN_MOVED_TO);
	if (wd == -1) {
		PERROR("inotify_add_watch", errno);
		return errno;
	}

	INFO("watching directory %s", cfg.watchdir);

	/* Allocate buffer for 1 event */
	event_buf = malloc(sizeof(*event) + PATH_MAX);
	if (!event_buf) {
		PERROR("malloc", ENOMEM);
		return ENOMEM;
	}
	event = (struct inotify_event *)event_buf;

	while ((err = get_inotify_event(fd, event)) == 0) {
		INFO("processing %s/%s", cfg.watchdir, event->name);
		err = chdir(cfg.watchdir);
		if (err == -1) {
			PERROR("chdir", errno);
			continue;
		}

		handle_inotify_event(event);
	}

	PERROR("get_inotify_event", err);
	return err;
}

int main (int argc, char *argv[])
{
	int err;

	parse_args(argc, argv);

	if (!(cfg.flags & DUPDATE_FLAG_FOREGROUND))
		daemonize();
	if (cfg.flags & DUPDATE_FLAG_DETACH) {
		ERROR("detach mode not implemented yet");
		exit(EXIT_FAILURE);
	}

	err = inotify_watch();
	if (err) {
		PERROR("inotify_watch", err);
		return err;
	}

	return 0;
}
