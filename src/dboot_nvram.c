#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <config.h>

#include "dboot.h"

struct dboot_nvram {
	char bl[80];
	char os_a[80];
	char os_b[80];
	char reserved[14];
	uint16_t status;
};

static int fd = 0;
static struct dboot_nvram shadow;

#define NVRAM_DEV "/dev/dboot_status"


int
dboot_backend_init(void)
{
	int err=0;

	fd = open(NVRAM_DEV, O_RDWR);
	if (fd == -1) {
		perror("open");
		err = errno;
		goto err_out;
	}

	err = read(fd, &shadow, sizeof(shadow));
	if (err == -1) {
		perror("read");
		err = errno;
		goto err_out;
	} else if (err != sizeof(shadow)) {
		fprintf(stderr, "partial read (%s): %d (%d expected)\n",
			NVRAM_DEV, err, sizeof(shadow));
		err = -EIO;
		goto err_out;
	}

	return 0;

err_out:
	dboot_backend_cleanup();
	return err;
}

void
dboot_backend_cleanup(void)
{
	if (fd > 0) {
		if (close(fd) == -1)
			perror("close");
		fd = 0;
	}
}

int
get_status_word(uint16_t * status_word)
{
	if (fd <= 0)
		return -EIO;

	*status_word = shadow.status;
	return 0;
}

int
set_status_word(uint16_t status_word)
{
	int err;

	if (fd <= 0)
		return -EIO;

	err = lseek(fd, (void *)&shadow.status - (void *)&shadow, SEEK_SET);
	if (err == -1) {
		perror("lseek");
		return errno;
	}

	err = write(fd, &status_word, sizeof(shadow.status));
	if (err == -1) {
		perror("write");
		return errno;
	} else if (err != sizeof(shadow.status)) {
		fprintf(stderr, "partial write: %d (%d expected)\n",
			err, sizeof(shadow.status));
		return -EIO;
	}

	shadow.status = status_word;

	return 0;
}

int
get_bl(char *buf)
{
	if (fd <= 0)
		return -EIO;

	strncpy(buf, shadow.bl, sizeof(shadow.bl));
	buf[sizeof(shadow.bl) - 1] = '\0';

	return 0;
}

int
get_os_a(char *buf)
{
	if (fd <= 0)
		return -EIO;

	strncpy(buf, shadow.os_a, sizeof(shadow.os_a));
	buf[sizeof(shadow.os_a) - 1] = '\0';

	return 0;
}

int
get_os_b(char *buf)
{
	if (fd <= 0)
		return -EIO;

	strncpy(buf, shadow.os_b, sizeof(shadow.os_b));
	buf[sizeof(shadow.os_b) - 1] = '\0';

	return 0;
}

int
set_bl(const char *bl)
{
	int err;

	if (fd <= 0)
		return -EIO;

	err = lseek(fd, (void *)shadow.bl - (void *)&shadow, SEEK_SET);
	if (err == -1) {
		perror("lseek");
		return errno;
	}

	strncpy(shadow.bl, bl, sizeof(shadow.bl));
	shadow.bl[sizeof(shadow.bl) - 1] = '\0';

	err = write(fd, shadow.bl, sizeof(shadow.bl));
	if (err == -1) {
		perror("write");
		return errno;
	} else if (err != sizeof(shadow.bl)) {
		fprintf(stderr, "partial write: %d (%d expected)\n",
			err, sizeof(shadow.bl));
		return -EIO;
	}

	return 0;
}

int
set_os_a(const char *os_a)
{
	int err;

	if (fd <= 0)
		return -EIO;

	err = lseek(fd, (void *)shadow.os_a - (void *)&shadow, SEEK_SET);
	if (err == -1) {
		perror("lseek");
		return errno;
	}

	strncpy(shadow.os_a, os_a, sizeof(shadow.os_a));
	shadow.os_a[sizeof(shadow.os_a) - 1] = '\0';

	err = write(fd, shadow.os_a, sizeof(shadow.os_a));
	if (err == -1) {
		perror("write");
		return errno;
	} else if (err != sizeof(shadow.os_a)) {
		fprintf(stderr, "partial write: %d (%d expected)\n",
			err, sizeof(shadow.os_a));
		return -EIO;
	}

	return 0;
}

int
set_os_b(const char *os_b)
{
	int err;

	if (fd <= 0)
		return -EIO;

	err = lseek(fd, (void *)shadow.os_b - (void *)&shadow, SEEK_SET);
	if (err == -1) {
		perror("lseek");
		return errno;
	}

	strncpy(shadow.os_b, os_b, sizeof(shadow.os_b));
	shadow.os_b[sizeof(shadow.os_b) - 1] = '\0';

	err = write(fd, shadow.os_b, sizeof(shadow.os_b));
	if (err == -1) {
		perror("write");
		return errno;
	} else if (err != sizeof(shadow.os_b)) {
		fprintf(stderr, "partial write: %d (%d expected)\n",
			err, sizeof(shadow.os_b));
		return -EIO;
	}

	return 0;
}
