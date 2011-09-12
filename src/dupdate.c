/*
 * Deif update Daemon
 *
 * This daemon application watches for new files in the defined
 * directory:
 * 
 * Author: Martin Lund (mgl@doredevelopment.dk)
 *         Morten Svendsen (mts@doredevelopment.dk)
 *
 * Copyright 2009 Event-Danmark A/S.
 * Copyright (C) 2010-2011 Prevas A/S
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define INOTIFY_PRESENT

/**** Includes ****************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <syslog.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>

/**** Application configuration ***********************************************/

#define VERSION		"v0.2"

/**** Status message macros ***************************************************/

#define INFO(format, args...) \
	fprintf (stdout, "" format, ## args)

#define ERROR(format, args...) \
	fprintf (stderr, "Error: " format, ## args)

#define INFO_LOG(format, args...) \
	syslog (LOG_INFO, "" format, ## args)

#define ERR_LOG(format, args...) \
	syslog (LOG_ERR, "" format, ## args)


/**** Defines *****************************************************************/

/* Allow for 4 simultanious events */
#define BUFF_SIZE	((sizeof(struct inotify_event)+PATH_MAX)*4)

/**** Global variables ********************************************************/

/* Configuration */
static struct {
	char dir[PATH_MAX];	/* Watched directory */
	char file[PATH_MAX];/* File to execute */
	int daemon;	/* Daemonize */
} config = {
	".",	/* Watched directory (default is . ) */
	"update",
	0,	/* Do not daemonize as default */
};

/**** Prototypes **************************************************************/

void handle_event (int fd, const char * target);
void handle_error (int error);
void print_help(char *argv[]);
static int parse_options(int argc, char *argv[]);
static void daemonize(void);

/**** Main ********************************************************************/

int main (int argc, char *argv[])
{
	int fd;	/* File descriptor */
	int wd;	/* Watch descriptor */

	/* Print usage help if no arguments */
	if (argc == 1)
	{
		print_help(argv);
		exit(0);
	}

	/* Check that it is run as 'root' user */
	if (getuid() != 0)
	{
		ERR_LOG("%s must be run as root\n",argv[0]);
		exit(1);
	}
	

	/* Set default directory to working directory */
	if (!getcwd(config.dir,PATH_MAX))
	{
		ERR_LOG("Path name is too long\n");
		exit(1);
	}

	/* Parse command line options */
	parse_options(argc, argv);

	/* Daemonize */
	if (config.daemon)
		daemonize();

	/* Initialize inotify instance */
	fd = inotify_init();
	if (fd == -1)
	{
		handle_error (errno);
		return 1;
	}

	/* Add watch on directory - only watch create events */
	wd = inotify_add_watch (fd, config.dir, IN_CLOSE_WRITE);
	if (wd == -1)
	{
		handle_error (errno);
		return 1;
	}

	/* Delete files if they already exists */

	INFO_LOG("Watching directory %s\n", config.dir);

	/* Enter event wait loop */
	while (1)
	{
		handle_event(fd, config.dir);
	}

	return 0;
}

/**** Functions ***************************************************************/

void write_pidfile(const char *path)
{
	int pid_fd;
	char buf[16];
	int n;

	if (!path)
		return;

	pid_fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (pid_fd < 0)
		return;
	
	n = sprintf(buf,"%d\n",getpid());
	
	if (write(pid_fd, buf, n) < 0)
		perror("writing pidfile");

	close(pid_fd);
}

void handle_event (int fd, const char * target)
{
	ssize_t len, i = 0;
	char buff[BUFF_SIZE] = {0};
	char str[BUFF_SIZE] = {0};
	char tmpd_mask[] = "/tmp/dupdate-XXXXXX";
	char tmpd[BUFF_SIZE] = {0};

	/* Read available events */
	len = read (fd, buff, BUFF_SIZE);

	while (i < len) {
		struct inotify_event *pevent = (struct inotify_event *)&buff[i];
		if ((pevent->len) && (pevent->mask & IN_CLOSE_WRITE))
		{
			if (chdir(config.dir))
				exit(1);

			strcpy(tmpd, tmpd_mask);

			if (mkdtemp(tmpd) == NULL)
			{
				handle_error(errno);
				return;
			}
			
			sprintf(str,"tar -C %s -xf %s",tmpd,pevent->name);
			INFO_LOG("Running %s\n",str); 

			if (system(str) != 0)
			{
				ERR_LOG("Error running %s\n",str);
				sprintf(str,"rm -rf %s",tmpd);
				if (system(str) != 0)
					INFO_LOG("Error runnnig %s\n",str);

				i += sizeof(struct inotify_event) + pevent->len;
				continue;
			}

			sprintf(str,"rm %s",pevent->name);
			if (system(str) != 0)
				INFO_LOG("Error runnnig %s\n",str);

			if (chdir(tmpd))
				exit(1);

			sprintf(str,"./%s",config.file);
			INFO_LOG("Tar extracted, Running %s in %s/\n",str,tmpd);
			if (system(str) != 0)
			{
				ERR_LOG("Error running %s\n",str);
				continue;
			}

			sprintf(str,"rm -rf %s",tmpd);
			if (system(str) != 0)
				INFO_LOG("Error runnnig %s\n",str);
		}

		i += sizeof(struct inotify_event) + pevent->len;
	}
}


void handle_error (int error)
{
	ERR_LOG ("Error: %s\n", strerror(error));
}


void print_help(char *argv[])
{
	INFO("Usage: %s [options]\n", argv[0]);
	INFO("--dir      <directory> Dir to watch for updatefiles\n");
	INFO("--daemon               Daemonize\n");
	INFO("--help                 Display help\n");
	INFO("\n");
}

static int parse_options(int argc, char *argv[])
{
	int c;

	while (1)
	{
		static struct option long_options[] =
		{
			{"dir",		required_argument,	0, 'd'},
			{"file",	required_argument,	0, 'f'},
			{"daemon",	no_argument,		0, 'z'},
			{"help",	no_argument,		0, 'h'},
			{0, 0, 0, 0}
		};
		
		/* getopt_long stores the option index here. */
		int option_index = 0;

		/* Parse argument using getopt_long */
		c = getopt_long (argc, argv, "", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
			case 0:
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0)
					break;
				INFO_LOG("option %s", long_options[option_index].name);
				if (optarg)
					INFO_LOG(" with arg %s", optarg);
				INFO_LOG("\n");
				break;

			case 'd':
				/* Obtain absolute path */
				if (!realpath(optarg, config.dir))
				{
					ERR_LOG("realpath failed\n");
					exit(1);
				}
				break;

			case 'f':
				strncpy(config.file,optarg,PATH_MAX);
				break;

			case 'z':
				config.daemon = 1;
				break;

			case 'h':
				print_help(argv);
				exit(0);
				break;

			case '?':
				/* getopt_long already printed an error message. */
				break;

			default:
				exit(1);
		}
	}

	/* Print any remaining command line arguments (not options). */
	if (optind < argc)
	{
		ERR_LOG("%s: unknown arguments: ", argv[0]);
		while (optind < argc)
			ERR_LOG("%s ", argv[optind++]);
		ERR_LOG("\n");
		exit(1);
	}
	return 0;
}

static void daemonize(void)
{
	pid_t pid, sid;
	
	/* Return if already a daemon */
	if ( getppid() == 1 ) return;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0)
	{
		exit(EXIT_FAILURE);
	}
	
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0)
	{
		exit(EXIT_SUCCESS);
	}

	/* At this point we are executing as the child process */

	write_pidfile("/var/run/dupdate.pid");

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0)
	{
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory.  This prevents the current
	directory from being locked; hence not being able to remove it. */
	if ((chdir("/")) < 0) {
		exit(EXIT_FAILURE);
	}

	/* Redirect standard files to /dev/null */
	if (freopen( "/dev/null", "r", stdin) == NULL)
		handle_error(errno);

	if (freopen( "/dev/null", "w", stdout) == NULL)
		handle_error(errno);

	if (freopen( "/dev/null", "w", stderr) == NULL)
		handle_error(errno);

}
