#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include "common.h"

#define QLEN 10
#define STALE 30 /* client's name can't be older than this (sec) */


/* APUE III Figure 17.8 */
int
s_unix_listen(const char *name)
{
	int fd, len, err, rval;
	struct sockaddr_un un;

	if (strlen(name) >= sizeof(un.sun_path)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -2;
	unlink(name);
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, name);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(name);
	if (bind(fd, (struct sockaddr *) &un, len) < 0) {
		rval = -3;
		goto errout;
	}
	if (listen(fd, QLEN) < 0) {
		rval = -4;
		goto errout;
	}
	return fd;

errout:
	err = errno;
	close(fd);
	errno = err;
	return rval;
}

/* APUE III Figure 17.9 */
int
s_unix_accept(int listenfd, uid_t *uidptr)
{
	int clifd, err, rval;
	socklen_t len;
	time_t staletime;
	struct sockaddr_un un;
	struct stat statbuf;
	char *name;

	if ((name = malloc(sizeof(un.sun_path + 1))) == NULL)
		return -1;
	len = sizeof(un);
	if ((clifd = accept(listenfd, (struct sockaddr *)&un, &len)) < 0) {
		free(name);
		return -2;
	}

	len -= offsetof(struct sockaddr_un, sun_path);
	memcpy(name, un.sun_path, len);
	name[len] = 0;
	if (stat(name, &statbuf) < 0) {
		rval = -3;
		goto errout;
	}

#ifdef S_ISSOCK
	if (S_ISSOCK(statbuf.st_mode) == 0) {
		rval = -4;
		goto errout;
	}
#endif

	if ((statbuf.st_mode & (S_IRWXG | S_IRWXO)) ||
			(statbuf.st_mode & S_IRWXU) !=  S_IRWXU) {
		rval = -5;
		goto errout;
	}

	staletime = time(NULL) - STALE;
	if (statbuf.st_atime < staletime ||
			statbuf.st_ctime < staletime ||
			statbuf.st_mtime < staletime) {
		rval = -6;
		goto errout;
	}

	if (uidptr != NULL)
		*uidptr = statbuf.st_uid;
	unlink(name);
	free(name);
	return clifd;

errout:
	err = errno;
	close(clifd);
	free(name);
	errno = err;
	return rval;
}

void *
alloc0(size_t size)
{
	void *mem;

	if ((mem = calloc(1, size)) != NULL)
		return mem;
	fputs("malloc failed\n", stderr);
	fflush(stderr);
	_exit(1);
}
