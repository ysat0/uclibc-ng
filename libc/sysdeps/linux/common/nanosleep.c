/*
 * nanosleep() for uClibc
 *
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <sys/syscall.h>
#include <time.h>
#include <cancel.h>

#if defined(__NR_nanosleep)
#define __NR___nanosleep_nocancel __NR_nanosleep
static _syscall2(int, __NC(nanosleep), const struct timespec *, req,
		 struct timespec *, rem);

CANCELLABLE_SYSCALL(int, nanosleep,
		    (const struct timespec *req, struct timespec *rem),
		    (req, rem))
#else
int nanosleep(const struct timespec *req, struct timespec *rem)
{
	errno = -ENOSYS;
	return -1;
}
int nanosleep_nocancel(const struct timespec *req, struct timespec *rem)
{
	errno = -ENOSYS;
	return -1;
}
#endif
lt_libc_hidden(nanosleep)
