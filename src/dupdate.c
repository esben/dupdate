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
#include <getopt.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/param.h>
#include <sys/wait.h>

#include "common.h"

#define DEFAULT_WORKDIR			"/tmp/dupdate-XXXXXX"
#define DEFAULT_CMDFILE			"run"

struct dupdate_args {
	char *workdir;		/* Working directory (mkdtemp template) */
	char *tarcmd;		/* Command to execute in tar archives */
	char *zipcmd;		/* Command to execute in zip archives*/
	int flags;		/* Configuration flags */
};

#define DUPDATE_FLAG_SYSLOG		(1 << 0)
#define DUPDATE_FLAG_REMOVE_IMAGE	(1 << 1)
#define DUPDATE_FLAG_REMOVE_WORKDIR	(1 << 2)
#define DUPDATE_FLAG_COMPLETION		(1 << 3)

static struct dupdate_args args;

enum dupdate_image_type {
	DUPDATE_IMAGE_TYPE_TAR,
	DUPDATE_IMAGE_TYPE_ZIP,
};

static char *workdir;
static char *image;		/* dupdate image file */
static enum dupdate_image_type image_type;
static char *completion_file;
static char *shcmd;
static int shcmd_len;
static const char *cwd;

static int create_workdir(void)
{
	char *tmpdir;

	tmpdir = strdup(args.workdir);
	if (!tmpdir) {
		PERROR("strdup", errno);
		return errno;
	}

	if (!mkdtemp(tmpdir)) {
		PERROR("mkdtemp", errno);
		return errno;
	}

	workdir = tmpdir;
	return 0;
}

static void remove_workdir(void)
{
	int err;

	if (!(args.flags & DUPDATE_FLAG_REMOVE_WORKDIR))
		return;

	err = chdir(cwd);
	if (err == -1) {
		PERROR("chdir", errno);
		return;
	}

	snprintf(shcmd, shcmd_len, "rm -rf \"%s\"", workdir);
	err = run_shcmd(shcmd);
	if (err)
		PERROR(shcmd, err);
}

static int remove_image(void)
{
	int err;
	struct stat st;

	if (!(args.flags & DUPDATE_FLAG_REMOVE_IMAGE))
		return 0;

	if (stat(image, &st) == -1) {
		if (errno == ENOENT)
			return 0;
		PERROR("stat", errno);
		return errno;
	}

	err = unlink(image);
	if (err == -1) {
		PERROR("unlink", errno);
		return errno;
	}

	return 0;
}

static int process_zip_image(void)
{
	int err;
	char *image_fullpath = NULL;

	snprintf(shcmd, shcmd_len, "unzip -q -d \"%s\" \"%s\" \"%s\"",
		 workdir, image, args.zipcmd);
	if ((err = run_shcmd(shcmd))) {
		PERROR(shcmd, err);
		goto out;
	}

	image_fullpath = realpath(image, NULL);
	if (!image_fullpath) {
		ERROR("could not find image file: %s", image);
		err = ENOENT;
		goto out;
	}

	err = chdir(workdir);
	if (err == -1) {
		PERROR("chdir", errno);
		err = errno;
		goto out;
	}

	err = chmod(args.zipcmd, S_IRUSR|S_IXUSR|S_IXGRP|S_IXOTH);
	if (err == -1) {
		PERROR("chmod", errno);
		err = errno;
		goto out;
	}

	snprintf(shcmd, shcmd_len, "\"./%s\" \"%s\"",
		 args.zipcmd, image_fullpath);
	if ((err = run_shcmd(shcmd))) {
		PERROR(shcmd, err);
		goto out;
	}

out:
	if (image_fullpath)
		free(image_fullpath);
	return err;
}

static int process_tar_image(void)
{
	int err;

	snprintf(shcmd, shcmd_len, "tar -x -C \"%s\" -f \"%s\"",
		 workdir, image);
	if ((err = run_shcmd(shcmd))) {
		PERROR(shcmd, err);
		goto out;
	}

	err = remove_image();
	if (err) {
		PERROR("remove_image", err);
		goto out;
	}

	err = chdir(workdir);
	if (err == -1) {
		PERROR("chdir", errno);
		err = errno;
		goto out;
	}

	snprintf(shcmd, shcmd_len, "\"./%s\"", args.tarcmd);
	if ((err = run_shcmd(shcmd))) {
		PERROR(shcmd, err);
		goto out;
	}

out:
	return err;
}

static int init_completion_file(void)
{
	if (!(args.flags & DUPDATE_FLAG_COMPLETION))
		return 0;

	/* Allocate room for string "image.success" and "image.fail" */
	completion_file = malloc(strlen(image) + 9);
	if (completion_file == NULL)
		return -ENOMEM;

	return 0;
}

static int remove_old_completion_files(void)
{
	if (!(args.flags & DUPDATE_FLAG_COMPLETION))
		return 0;

	/* Cleanup existing completion files */
	sprintf(completion_file, "%s.success", image);
	unlink(completion_file);
	sprintf(completion_file, "%s.fail", image);
	unlink(completion_file);

	return 0;
}

static void write_completion_file(int err)
{
	if (!(args.flags & DUPDATE_FLAG_COMPLETION))
		return;

	if (err)
		sprintf(completion_file, "%s.fail", image);
	else
		sprintf(completion_file, "%s.success", image);
	open(completion_file, O_WRONLY|O_CREAT,
	     S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
}

static int guess_image_type(void)
{
	int fd, ret;
	char buf[4];

	fd = open(image, O_RDONLY);
	if (fd == -1) {
		PERROR("open", errno);
		return 1;
	}

	ret = read(fd, buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		if (ret == -1) {
			PERROR("read", errno);
		}
		else {
			ERROR("partial archive head read: %d", ret);
		}
		if (close(fd) == -1) {
			PERROR("close", errno);
		}
		return 1;
	}

	if (close(fd) == -1) {
		PERROR("close", errno);
	}

	if (buf[0]==0x50 && buf[1]==0x4b && buf[2]==0x03 && buf[3]==0x04)
		image_type = DUPDATE_IMAGE_TYPE_ZIP;
	else
		/* Tar-balls does not have a single header to look for.
		 * Compressed tarballs are simply compressed files, which
		 * happens to be a tar file, so you will fx. not be able to
		 * differentiate between a tar.gz and any other gzip
		 * compressed file without actually decompressing the file,
		 * and looking of the header of the decompressed file.
		 *
		 * So we simply assume that anything that is not a zip file is
		 * a tar-ball, and let the tar command (try to) handle it. */
		image_type = DUPDATE_IMAGE_TYPE_TAR;

	return 0;
}

static int process_image(void)
{
	int err;

	err = remove_old_completion_files();
	if (err)
		goto early_out;

	err = create_workdir();
	if (err) {
		ERROR("workdir creation failed");
		goto create_workdir_failed;
	}

	err = guess_image_type();
	if (err)
		goto out;

	switch (image_type) {
	case DUPDATE_IMAGE_TYPE_TAR:
		err = process_tar_image();
		break;
	case DUPDATE_IMAGE_TYPE_ZIP:
		err = process_zip_image();
		break;
	default:
		ERROR("Unexpected image_type");
		err = -EINVAL;
		goto out;
	}

	chdir(cwd);

out:
	remove_workdir();
create_workdir_failed:
	write_completion_file(err);
early_out:
	if (err) {
		INFO("FAILURE: %s", image);
	} else {
		INFO("SUCCESS: %s", image);
	}

	remove_image();

	return err;
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

static const char *usage  = "\
Usage: %s [OPTIONS] <FILE>\n\n\
Arguments:\n\
  <IMAGE>               Process dupdate IMAGE file\n\
Options:\n\
  -d, --workdir=<TEMPLATE>  Create temporary directory based on TEMPLATE and\n\
			use it as working directory \n\
			[default: /tmp/dupdate-XXXXXX]\n\
  -x, --tarcmd=<FILE>   FILE in tar archives to execute [default: run]\n\
  -z, --zipcmd=<FILE>   FILE in zip archives to execute [default: run]\n\
  -l, --syslog          Output syslog instead of stdout/stderr\n\
  -R, --no-remove       Don't remove image after unpacking\n\
  -C, --no-cleanup      Don't remove unpacked files when done\n\
  -c, --completion      Create completion file when done\n\
  --help                Display help\n\
";

static const struct option longopts[] = {
	{"workdir",	required_argument,	NULL, 'd'},
	{"tarcmd",	required_argument,	NULL, 'x'},
	{"zipcmd",	required_argument,	NULL, 'z'},
	{"syslog",	no_argument,		NULL, 'l'},
	{"no-remove",	no_argument,		NULL, 'R'},
	{"no-cleanup",	no_argument,		NULL, 'C'},
	{"completion",	no_argument,		NULL, 'c'},
	{"help",	no_argument,		NULL, 'h'},
	{NULL,		0,			NULL,  0 }
};

static const char *optstring = "d:x:z:lRCch";

static void parse_args(int argc, char *argv[])
{
	int opt, longindex, err;

	args.flags = DUPDATE_FLAG_REMOVE_IMAGE | DUPDATE_FLAG_REMOVE_WORKDIR;

	/* Parse argument using getopt_long */
	while ((opt = getopt_long(argc, argv, optstring, longopts,
				  &longindex)) != -1) {
		int err = 0;

		switch (opt) {

		case 'd':
			err = strset(&args.workdir, optarg, PATH_MAX - 7);
			if (err) {
				PERROR("strset", err);
			}
			break;

		case 'x':
			err = strset(&args.tarcmd, optarg, NAME_MAX);
			if (err) {
				PERROR("strset", err);
			}
			break;

		case 'z':
			err = strset(&args.zipcmd, optarg, NAME_MAX);
			if (err) {
				PERROR("strset", err);
			}
			break;

		case 'l':
			args.flags |= DUPDATE_FLAG_SYSLOG;
			break;

		case 'R':
			args.flags &= ~DUPDATE_FLAG_REMOVE_IMAGE;
			break;

		case 'C':
			args.flags &= ~DUPDATE_FLAG_REMOVE_WORKDIR;
			break;

		case 'c':
			args.flags |= DUPDATE_FLAG_COMPLETION;
			break;

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

	if (optind >= argc) {
		ERROR("file argument missing");
		exit(EXIT_FAILURE);
	} else if ((argc - optind) > 1) {
		ERROR("too many arguments");
		exit(EXIT_FAILURE);
	}

	err = strset(&image, argv[optind], PATH_MAX);
	if (err) {
		PERROR("strset", err);
		exit(EXIT_FAILURE);
	}

	err = init_completion_file();
	if (err) {
		PERROR("init_completion_file", err);
		exit(EXIT_FAILURE);
	}

	if (!args.workdir)
		args.workdir = DEFAULT_WORKDIR;
	if (!args.tarcmd)
		args.tarcmd = DEFAULT_CMDFILE;
	if (!args.zipcmd)
		args.zipcmd = DEFAULT_CMDFILE;
}

int main(int argc, char *argv[])
{
	parse_args(argc, argv);

	if (args.flags & DUPDATE_FLAG_SYSLOG)
		log_to_syslog(1);

	if (get_sysconf())
		exit(EXIT_FAILURE);

	return process_image();
}
