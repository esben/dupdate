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
#include <sys/wait.h>

struct dupdate_config cfg;

static char *tmpdir_template;
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
	struct stat st;

	if (cfg.flags & DUPDATE_FLAG_NO_REMOVE)
		return 0;

	err = chdir(cfg.watchdir);
	if (err == -1) {
		PERROR("chdir", errno);
		return errno;
	}

	if (stat(path, &st) == -1) {
		if (errno == ENOENT)
			return 0;
		PERROR("stat", errno);
		return errno;
	}

	err = unlink(path);
	if (err == -1) {
		PERROR("unlink", errno);
		return errno;
	}

	return 0;
}

static char *
new_tmpdir(void)
{
	char *tmpdir;

	tmpdir = strdup(tmpdir_template);
	if (!tmpdir) {
		PERROR("strdup", errno);
		return NULL;
	}

	strcpy(tmpdir, tmpdir_template);
	if (!mkdtemp(tmpdir)) {
		PERROR("mkdtemp", errno);
		free(tmpdir);
		return NULL;
	}

	return tmpdir;
}

static int
destroy_tmpdir(const char *tmpdir)
{
	int err;

	if (cfg.flags & DUPDATE_FLAG_NO_CLEANUP)
		return 0;

	err = chdir(cfg.watchdir);
	if (err == -1) {
		PERROR("chdir", errno);
		return errno;
	}

	snprintf(shcmd, shcmd_len, "rm -rf %s", tmpdir);
	INFO("running: %s", shcmd);
	err = system(shcmd);
	if (err == -1) {
		PERROR(shcmd, errno);
		return errno;
	}

	return 0;
}

static int
process_zip_archive(const char *name)
{
	int err;
	char *tmpdir;

	tmpdir = new_tmpdir();
	if (!tmpdir) {
		ERROR("tmpdir creation failed");
		return 1;
	}

	INFO("processing zip archive in %s", tmpdir);
	err = chdir(tmpdir);
	if (err == -1) {
		PERROR("chdir", errno);
		goto out;
	}

	snprintf(shcmd, shcmd_len, "unzip -q \"%s/%s\" %s",
		 cfg.watchdir, name, cfg.zipcmd);
	INFO("running: %s", shcmd);
	err = system(shcmd);
	if (err == -1) {
		PERROR(shcmd, errno);
		goto out;
	} else if (err) {
		ERROR("%s: exit status: %d", shcmd, WEXITSTATUS(err));
		goto out;
	}

	err = chmod(cfg.zipcmd, S_IRUSR|S_IXUSR|S_IXGRP|S_IXOTH);
	if (err == -1) {
		PERROR("chmod", errno);
		goto out;
	}

	snprintf(shcmd, shcmd_len, "./%s %s/%s",
		 cfg.zipcmd, cfg.watchdir, name);
	INFO("running: %s", shcmd);
	err = system(shcmd);
	if (err == -1) {
		PERROR(shcmd, errno);
		goto out;
	} else if (err) {
		ERROR("%s: exit status: %d", shcmd, WEXITSTATUS(err));
		goto out;
	}

out:
	destroy_tmpdir(tmpdir);
	free(tmpdir);
	return err ? 1 : 0;
}

static int
process_tar_archive(const char *name)
{
	int err;
	char *tmpdir;

	INFO("processing tar archive in %s", tmpdir);

	tmpdir = new_tmpdir();
	if (!tmpdir) {
		ERROR("tmpdir creation failed");
		err = 1;
		goto err_early;
	}

	err = chdir(tmpdir);
	if (err == -1) {
		PERROR("chdir", errno);
		goto out;
	}

	snprintf(shcmd, shcmd_len, "tar -xf \"%s/%s\"", cfg.watchdir, name);
	INFO("running: %s", shcmd);
	err = system(shcmd);
	if (err == -1) {
		PERROR(shcmd, errno);
		goto out;
	} else if (err) {
		ERROR("%s: exit status: %d", shcmd, WEXITSTATUS(err));
		goto out;
	}

	remove_archive(name);

	err = chdir(tmpdir);
	if (err == -1) {
		PERROR("chdir", errno);
		goto out;
	}

	snprintf(shcmd, shcmd_len, "./%s", cfg.tarcmd);
	INFO("running: %s", shcmd);
	err = system(shcmd);
	if (err == -1) {
		PERROR(shcmd, errno);
		goto out;
	} else if (err) {
		ERROR("%s: exit status: %d", shcmd, WEXITSTATUS(err));
		goto out;
	}

out:
	destroy_tmpdir(tmpdir);
	free(tmpdir);
err_early:
	remove_archive(name);
	return err ? 1 : 0;
}

static int
process_archive(const char *name)
{
	int fd, ret;
	char buf[4];

	fd = open(name, O_RDONLY);
	if (fd == -1) {
		PERROR("open", errno);
		return 1;
	}

	ret = read(fd, buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		if (ret == -1)
			PERROR("read", errno);
		else
			ERROR("partial archive head read: %d", ret);
		if (close(fd) == -1)
			PERROR("close", errno);
		return 1;
	}

	if (close(fd) == -1)
		PERROR("close", errno);

	if (buf[0]==0x50 && buf[1]==0x4b && buf[2]==0x03 && buf[3]==0x04)
		return process_zip_archive(name);
	else
		return process_tar_archive(name);
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
	shcmd = malloc(shcmd_len);
	if (!tmpdir_template || !shcmd) {
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
		if (event->len == 0) {
			ERROR("ignoring anonymous event");
			return;
		}

		INFO("processing %s/%s", cfg.watchdir, event->name);

		err = chdir(cfg.watchdir);
		if (err == -1) {
			PERROR("chdir", errno);
			continue;
		}

		if (process_archive(event->name))
			INFO("FAILURE: %s", event->name);
		else
			INFO("SUCCESS: %s", event->name);

		remove_archive(event->name);
	}

	PERROR("get_inotify_event", err);
	return err;
}

int main (int argc, char *argv[])
{
	int err;

	parse_args(argc, argv);

#ifdef HAVE_FORK
	if (!(cfg.flags & DUPDATE_FLAG_FOREGROUND))
		daemonize();
	if (cfg.flags & DUPDATE_FLAG_DETACH) {
		ERROR("detach mode not implemented yet");
		exit(EXIT_FAILURE);
	}
#endif /* HAVE_FORK */

	err = inotify_watch();
	if (err) {
		PERROR("inotify_watch", err);
		return err;
	}

	return 0;
}
