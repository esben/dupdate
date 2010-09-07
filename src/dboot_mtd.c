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

#define MTD_NAME "dboot-status"
#define MTD_SYS "/sys/class/mtd/mtd%d/name"
#define MTD_DEV "/dev/mtd%d"

static int fd;

int
dboot_backend_init(void)
{
	char dev[128];
	char name[128];
	int ret = 0;
	int i;
	int mtd_fd;

	for (i=0;i<32;i++) {
		sprintf(dev,MTD_SYS,i);
		mtd_fd = open(dev, O_RDONLY);
		if (mtd_fd == -1) {
			continue;
		} else {
			ret = read(mtd_fd, name, 64);
			close(mtd_fd);
			if (ret < 1)
				continue;

			name[ret-1] = 0;

			if (strcmp(name, MTD_NAME) == 0) {
				ret = 0;
				break;
			}
		}
	}

	if (ret) {
		fprintf(stderr, "Cannot find '%s' mtd device\n",MTD_NAME);
		ret = -1;
		goto end;
	}

	sprintf(dev,MTD_DEV,i);
	if ((fd = open(dev, O_RDWR)) < 0) {
		fprintf(stderr,
			"%s\n",
			strerror(errno));
		ret = 1;
		goto end;
	}

end:
	return ret;
}

void
dboot_backend_cleanup(void)
{
}

int
erase_all(void)
{
	mtd_info_t meminfo;
	erase_info_t erase;
	int ret;

	ret = ioctl(fd, MEMGETINFO, &meminfo);
	if ( ret != 0) {
		fprintf(stderr, "unable to get MTD device info\n");
		goto end;
	}

	erase.length = meminfo.erasesize;

	for (erase.start = 0;
	     erase.start < meminfo.size;
	     erase.start += meminfo.erasesize){

		ret = ioctl(fd, MEMERASE, &erase);
		if (ret != 0) {
			fprintf(stderr,
				"\nMTD Erase failure: %s\n",
				strerror(errno));
			goto end;
		}
	}

	ret = lseek(fd,0,SEEK_SET);
	if (ret == -1) {
		fprintf(stderr, "unable to seek\n");
		goto end;
	}

end:
	return ret;
}

int
get_status_word(uint16_t * status_word)
{
	int ret = 0;
	uint16_t status;

	ret = lseek(fd, 0, SEEK_SET);
	if (ret != 0) {
		fprintf(stderr, "unable to seek\n");
		goto end;
	}

	while ((ret = read(fd,&status,sizeof(uint16_t))) == sizeof(uint16_t) &&
		status != 0xffff) {
		*status_word = status;
	}

	/*eof get last status word erase block and seek to start*/
	if (ret == 0) {
		ret = lseek(fd,-sizeof(uint16_t),SEEK_END);
		if (ret == -1) {
			fprintf(stderr, "unable to seek\n");
			goto end;
		}

		ret = read(fd,&status,sizeof(uint16_t));
		if (ret != sizeof(uint16_t)) {
			fprintf(stderr, "unable to seek\n");
			ret = 1;
			goto end;
		}

		ret = erase_all();
		if (ret != 0) {
			fprintf(stderr, "erase_all fail\n");
			goto end;
		}

		ret = write(fd,&status,sizeof(uint16_t));
		if (ret != sizeof(uint16_t)) {
			fprintf(stderr, "Unable to write %#x\n",
				status);
			goto end;
		}
		*status_word = status;
	} else if (ret < 0) {
		fprintf(stderr,
			"\nMTD read failure: %s\n",
			strerror(errno));
		goto end;
	} else if (status == 0xffff) {
		ret = lseek(fd,-sizeof(uint16_t),SEEK_CUR);
		if (ret == -1) {
			fprintf(stderr, "unable to seek\n");
			goto end;
		}
		ret = 0;
	} else {
		fprintf(stderr,
			"Error finding valid status word: %s\n",
			strerror(errno));
		ret = 1;
		goto end;
	}

end:
	return ret;
}

int
set_status_word(uint16_t status_word)
{
	int ret;

	ret = write(fd,&status_word,sizeof(uint16_t));
	if (ret < 0) {
		perror("Unable to write status_word");
		goto end;
	} else if (ret != sizeof(uint16_t)) {
		fprintf(stderr, "Partial status_word write: %d\n", ret);
		goto end;
	} else  {
		ret = 0;
	}
	fsync(fd);

end:
	return ret;
}

int
get_bl(char *buf)
{
	buf[0] = '\0';
	return 0;
}

int
get_os_a(char *buf)
{
	buf[0] = '\0';
	return 0;
}

int
get_os_b(char *buf)
{
	buf[0] = '\0';
	return 0;
}

int
set_bl(const char *buf)
{
	return 0;
}

int
set_os_a(const char *buf)
{
	return 0;
}

int
set_os_b(const char *buf)
{
	return 0;
}
