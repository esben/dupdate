/*
 * Command for getting boot flags.
 *
 * Copyright (C) 2010-2011 Prevas A/S
 *
 * Author: Morten Thunberg Svendsen <mts@doredevelopment.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#define DBOOT_BL		0x0001U
#define DBOOT_DL		0x0002U
#define DBOOT_ALT		0x0004U
#define DBOOT_OS		0x0008U // only used in boot_selection
#define DBOOT_DEFAULT_DL	0x0010U
#define DBOOT_DEFAULT_OS	0x0020U
#define DBOOT_VALID		0x8000U
#define DBOOT_REBOOT		DBOOT_VALID

#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <libgen.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <mtd/mtd-user.h>
#include <unistd.h>
#include <signal.h>

#include "dboot.h"

static uint16_t status_word;

static int
get_cmdline(const char *string)
{
	char buf[1024];
	char digit[2];
	char *strp;
	int ret;

	FILE *fp = fopen("/proc/cmdline","r");
	if (fp == NULL) {
		fprintf(stderr,	"Open /proc/cmdline: %s\n", strerror(errno));
		exit(1);
	}

	ret = fread(buf, 1, 1024, fp);
	if (ret < 1) {
		fprintf(stderr,	"Read /proc/cmdline failed\n");
		exit(1);
	}
	buf[ret-1] = 0; /*last char in cmdline is a newline set to 0 */

	strp = strstr(buf,string);
	if (strp == NULL) {
		fprintf(stderr,	"Find %s in %s failed\n",string,buf);
		exit(1);
	}
	strp += strlen(string);
	digit[0] = strp[0];
	digit[1] = 0;
	return atoi(digit);
}

static const char usage[] = "Usage: dboot [OPTION]...\n"
"Display and change current dboot settings.\n"
"\nPrimary boot selection options (mutually exclusive):\n"
"  -b, --bl               next (re)boot should be to bootloader mode\n"
"  -d, --dl               next (re)boot should be to download mode\n"
"  -o, --os               next (re)boot should be to os mode\n"
"\nSecondary boot selection options (allowed in combination with a primary\n"
"boot selection option):\n"
"  -a, --alternative      next (re)boot should go to alternative image\n"
"  -r, --reboot           reboot NOW!\n"
"\nDefault boot selector options:\n"
"  -s, --set-default[=IMAGE]\n"
"                         set default image (both os and download mode).\n"
"                           if IMAGE is not specified, use current image.\n"
"                           otherwise, use the IMAGE specified\n"
"  -t, --set-default-dl[=IMAGE]\n"
"                         set default download mode image.\n"
"                           if IMAGE is not specified, use current image.\n"
"                           otherwise, use the IMAGE specified\n"
"  -T, --set-default-os[=IMAGE]\n"
"                         set default os mode image.\n"
"                           if IMAGE is not specified, use current image.\n"
"                           otherwise, use the IMAGE specified\n"
"\nQuery options:\n"
"  -g, --get-default      get default image (os and download mode)\n"
"  -h, --get-default-dl   get default download mode image\n"
"  -H, --get-default-os   get default os mode image\n"
"  -c, --get-current      get current image\n"
"  -D, --in-dl            in download mode (0=no, 1=yes)\n"
"  -A, --in-alternative   in alternative image (0=no, 1=yes)\n"
"  -M, --set-bl=TEXT      set bootloader image description/version\n"
"  -n, --set-os-a=TEXT    set os image a description/version\n"
"  -N, --set-os-b=TEXT    set os image b description/version\n"
"  -l, --list             list image version of bootloader and os images\n"
"  -m, --list-bl          display bootloader image version\n"
"  -L, --list-os[=IMAGE]\n"
"                         display os image versions\n"
"\nOther options:\n"
"  -h, --help             display this help message and exit\n";

static struct option getopt_longopts[] = {
	{"bl",			no_argument,		0, 'b'},
	{"dl",			no_argument,		0, 'd'},
	{"os",			no_argument,		0, 'o'},
	{"alternative",		no_argument,		0, 'a'},
	{"reboot",		no_argument,		0, 'r'},
	{"set-default",		optional_argument,	0, 's'},
	{"set-default-dl",	optional_argument,	0, 'T'},
	{"set-default-os",	optional_argument,	0, 't'},
	{"get-default",		no_argument,		0, 'g'},
	{"get-default-dl",	no_argument,		0, 'E'},
	{"get-default-os",	no_argument,		0, 'e'},
	{"get-current",		no_argument,		0, 'c'},
	{"in-dl",		no_argument,		0, 'D'},
	{"in-alternative",	no_argument,		0, 'A'},
	{"set-bl",		required_argument,	0, 'M'},
	{"set-os-a",		required_argument,	0, 'n'},
	{"set-os-b",		required_argument,	0, 'N'},
	{"list",		no_argument,		0, 'l'},
	{"list-bl",		no_argument,		0, 'm'},
	{"list-os",		optional_argument,	0, 'L'},
	{"help",		no_argument,		0, 'h'},
	{0, 0, 0, 0}
};

static const char * getopt_optstring = "bdoars::T::t::gEecDAM:n:N:lmL::h";

static int
parse_options(int argc, char * const argv[], uint16_t * status, int * reboot)
{
	int opt, err = 0;
	uint16_t new_status, boot_selection = 0;
	int ubivol, dlmode, alternative;

	ubivol = get_cmdline("ubivol=");
	if (ubivol < 0 || ubivol > 1) {
		fprintf(stderr, "Unable to detect current image from kernel commandline\n");
		return -EIO;
	}

	dlmode = get_cmdline("dlmode=");
	if (dlmode < 0 || dlmode > 1) {
		fprintf(stderr, "Unable to detect mode from kernel commandline\n");
		return -EIO;
	}

	alternative = get_cmdline("alternative=");
	if (alternative < 0 || alternative > 1) {
		fprintf(stderr, "Unable to detect alternative from kernel commandline\n");
		return -EIO;
	}

	new_status = *status;

	while (1)
	{
		/*
		  getopt returns -1 when there are no more option chars
		  optarg points to optional argument or is NULL
		  getopt returns '?' on invalid option char (stored in optopt)
		*/

		/* getopt_long stores the option index here. */
		int longindex = 0;

		/* Parse argument using getopt_long */
		opt = getopt_long(argc, argv,
				  getopt_optstring, getopt_longopts,
				  &longindex);

		/* Detect the end of the options. */
		if (opt == -1)
			break;

		switch (opt) {

		case 'b': // --bl
			boot_selection |= DBOOT_BL;
			break;

		case 'd': // --dl
			boot_selection |= DBOOT_DL;
			break;

		case 'o': // --os
			boot_selection |= DBOOT_OS;
			break;

		case 'a': // --alternative
			boot_selection |= DBOOT_ALT;
			break;

		case 'r': // --reboot
			boot_selection |= DBOOT_REBOOT;
			break;

		case 's': // --set-default[=IMAGE]
			if (optarg == NULL) {
				if (ubivol == 0) {
					new_status &= ~DBOOT_DEFAULT_DL;
					new_status &= ~DBOOT_DEFAULT_OS;
				} else {
					new_status |= DBOOT_DEFAULT_DL;
					new_status |= DBOOT_DEFAULT_OS;
				}
			} else if (strcmp("a", optarg) == 0) {
				new_status &= ~DBOOT_DEFAULT_DL;
				new_status &= ~DBOOT_DEFAULT_OS;
			} else if (strcmp("b", optarg) == 0) {
				new_status |= DBOOT_DEFAULT_DL;
				new_status |= DBOOT_DEFAULT_OS;
			} else {
				printf("Unsupported option %c to %s\n", *optarg,
				       getopt_longopts[longindex].name);
				err = -EINVAL;
			}
			break;

		case 'T': // --set-default-dl[=IMAGE]
			if (optarg == NULL) {
				if (ubivol == 0) {
					new_status &= ~DBOOT_DEFAULT_DL;
				} else {
					new_status |= DBOOT_DEFAULT_DL;
				}
			} else if (strcmp("a", optarg) == 0) {
				new_status &= ~DBOOT_DEFAULT_DL;
			} else if (strcmp("b", optarg) == 0) {
				new_status |= DBOOT_DEFAULT_DL;
			} else {
				printf("Unsupported option %c to %s\n", *optarg,
				       getopt_longopts[longindex].name);
				err = -EINVAL;
			}
			break;

		case 't': // --set-default-os[=IMAGE]
			if (optarg == NULL) {
				if (ubivol == 0) {
					new_status &= ~DBOOT_DEFAULT_OS;
				} else {
					new_status |= DBOOT_DEFAULT_OS;
				}
			} else if (strcmp("a", optarg) == 0) {
				new_status &= ~DBOOT_DEFAULT_OS;
			} else if (strcmp("b", optarg) == 0) {
				new_status |= DBOOT_DEFAULT_OS;
			} else {
				printf("Unsupported option %c to %s\n", *optarg,
				       getopt_longopts[longindex].name);
				err = -EINVAL;
			}
			break;

		case 'g': // --get-default
			if (*status & DBOOT_DEFAULT_OS) {
				if (*status & DBOOT_DEFAULT_DL)
					printf("b\n");
				else
					printf("b a\n");
			} else {
				if (*status & DBOOT_DEFAULT_DL)
					printf("a b\n");
				else
					printf("a\n");
			}
			break;

		case 'E': // --get-default-dl
			if (*status & DBOOT_DEFAULT_DL)
				printf("b\n");
			else
				printf("a\n");
			break;

		case 'e': // --get-default-os
			if (*status & DBOOT_DEFAULT_OS)
				printf("b\n");
			else
				printf("a\n");
			break;

		case 'c': // --get-current
			if (ubivol == 0)
				printf("a\n");
			else
				printf("b\n");
			break;

		case 'D': // --in-dl
			printf("%d\n", dlmode);
			break;

		case 'A': { // --in-alternative
			int in_a, default_a;
			in_a = (ubivol == 0);
			if (dlmode)
				default_a = !(*status & DBOOT_DEFAULT_DL);
			else
				default_a = !(*status & DBOOT_DEFAULT_OS);
			printf("%d\n", in_a != default_a);
			break;
		}

		case 'M': // --set-bl
			err = set_bl(optarg);
			break;

		case 'n': // --set-os-a
			err = set_os_a(optarg);
			break;

		case 'N': // --set-os-b
			err = set_os_b(optarg);
			break;

		case 'l': // --list
			err = list_all();
			break;

		case 'm': // --list-bl
			err = list_bl();
			break;

		case 'L': // --list-os[=IMAGE]
			if (optarg == NULL)
				err = list_os();
			else if (strcmp("a", optarg) == 0)
				err = list_os_a();
			else if (strcmp("b", optarg) == 0)
				err = list_os_b();
			else {
				printf("Unsupported option %c to %s\n", *optarg,
				       getopt_longopts[longindex].name);
				err = -EINVAL;
			}
			break;

		case 'h': // --help
		default:
			printf(usage);
			exit(0);
		}
	}

	if (err)
		return err;

	/* Print any remaining command line arguments (not options). */
	if (optind < argc)
	{
		fprintf(stderr, "%s: unknown arguments: ", argv[0]);
		while (optind < argc)
			fprintf(stderr, "%s ", argv[optind++]);
		fprintf(stderr, "\n");
		return -EINVAL;
	}

	if (boot_selection)
		new_status &= ~(DBOOT_BL|DBOOT_DL|DBOOT_ALT);

	if (boot_selection & DBOOT_BL) {
		printf("boot_selection=%04hx\n", boot_selection);
		if (boot_selection & (DBOOT_DL|DBOOT_OS|DBOOT_ALT)) {
			fprintf(stderr, "Invalid boot selection arguments: %x\n", boot_selection);
			return -EINVAL;
		}
		new_status |= boot_selection & DBOOT_BL;
	}

	else if (boot_selection & DBOOT_DL) {
		if (boot_selection & DBOOT_OS) {
			fprintf(stderr, "Invalid boot selection arguments: %x\n", boot_selection);
			return -EINVAL;
		}
		new_status |= boot_selection & (DBOOT_DL|DBOOT_ALT);
	}

	else if (boot_selection) {
		new_status |= boot_selection & DBOOT_ALT;
	}

	if (boot_selection & DBOOT_REBOOT)
		*reboot = 1;

	*status = new_status;

	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	uint16_t status, old_status;
	int reboot=0;

	err = dboot_backend_init();
	if (err)
		goto out;

	err = get_status_word(&status);
	if (err != 0)
		goto out;

	old_status = status;

	err = parse_options(argc, argv, &status, &reboot);
	if (err) {
		fprintf(stderr, "Failed to parse commandline arguments: %d\n", err);
		goto out;
	}

	if (status != old_status) {
		err = set_status_word(status);
		if (err)
			goto out;
	}

	if (reboot)
		kill(1, SIGINT);

out:
	dboot_backend_cleanup();

	return err;
}
