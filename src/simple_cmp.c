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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv)
{
	int fd1, fd2;
	unsigned long int len,i;
	ssize_t ret;
	char *buf1;
	char *buf2;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s file1 file2 len\n", argv[0]);
		return 1;
	}

	errno = 0;
	len = strtoul(argv[3],NULL,10);
	if (errno) {
		fprintf(stderr, "Error stroul %s : %s\n",argv[3],strerror(errno));
		return errno;
	}

	buf1 = malloc(len);
	if (buf1 == NULL) {
		fprintf(stderr, "Error malloc %lu : %s\n",len,strerror(errno));
		return errno;
	}

	buf2 = malloc(len);
	if (buf2 == NULL) {
		fprintf(stderr, "Error malloc %lu : %s\n",len,strerror(errno));
		return errno;
	}

	fd1 = open(argv[1], O_RDONLY);
	if ( fd1 < 0 ) {
		fprintf(stderr, "Error opening %s : %s\n",argv[1],strerror(errno));
		return errno;
	}

	fd2 = open(argv[2], O_RDONLY);
	if ( fd2 < 0 ) {
		fprintf(stderr, "Error opening %s : %s\n",argv[2],strerror(errno));
		return errno;
	}

	ret = read(fd1, buf1, len);
	if (ret < 0) {
		fprintf(stderr, "Error reading from %s : %s\n",argv[1],strerror(errno));
		return errno;
	} else if (len != ret) {
		fprintf(stderr, "Error reading %lu bytes from %s\n",len,argv[1]);
		return 1;
	}

	ret = read(fd2, buf2, len);
	if (ret < 0) {
		fprintf(stderr, "Error reading from %s : %s\n",argv[2],strerror(errno));
		return errno;
	} else if (len != ret) {
		fprintf(stderr, "Error reading %lu bytes from %s\n",len,argv[2]);
		return 1;
	}

	for (i=0;i<len;i++)
		if (buf1[i] != buf2[i]) {
			fprintf(stderr, "Error byte %lu in %s(%#x) and %s(%#x) did not match\n",
				i,argv[1],buf1[i],argv[2],buf2[i]);
			return 5;
		}

	return 0;
}
