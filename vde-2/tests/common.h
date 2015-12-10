#include <sys/types.h>

#ifndef __COMMON_H
#define __COMMON_H

int s_unix_listen(const char *name);
int s_unix_accept(int listenfd, uid_t *uidptr);
void *alloc0(size_t size);

#endif /* __COMMON_H */
