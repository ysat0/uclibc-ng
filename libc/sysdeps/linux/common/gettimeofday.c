/*
 * gettimeofday() for uClibc
 *
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <sys/syscall.h>
#include <sys/time.h>

#if defined(__NR_gettimeofday)
_syscall2(int, gettimeofday, struct timeval *, tv, __timezone_ptr_t, tz)
#else
int gettimeofday(struct timeval *tv, __timezone_ptr_t tz)
{
	errno = -ENOSYS;
	return -1;
}
#endif
libc_hidden_def(gettimeofday)
