/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include <autoconf.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sos.h>
#include <utils/attribute.h>
#include <sel4/sel4.h>

#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <sys/types.h>
#include <sys/syscall.h>
#include "ttyout.h"

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

long sys_writev(va_list ap)
{
    int fildes = va_arg(ap, int);
    struct iovec *iov = va_arg(ap, struct iovec *);
    int iovcnt = va_arg(ap, int);

    long long sum = 0;
    ssize_t ret = 0;

    /* The iovcnt argument is valid if greater than 0 and less than or equal to IOV_MAX. */
    if (iovcnt <= 0 || iovcnt > IOV_MAX) {
        return -EINVAL;
    }

    /* The sum of iov_len is valid if less than or equal to SSIZE_MAX i.e. cannot overflow
       a ssize_t. */
    for (int i = 0; i < iovcnt; i++) {
        sum += (long long)iov[i].iov_len;
        if (sum > SSIZE_MAX) {
            return -EINVAL;
        }
    }

    /* If all the iov_len members in the array are 0, return 0. */
    if (!sum) {
        return 0;
    }

    /* Write the buffer to console if the fd is for stdout or stderr. */
    if (fildes == STDOUT_FD || fildes == STDERR_FD) {
        for (int i = 0; i < iovcnt; i++) {
            ret += sos_write(iov[i].iov_base, iov[i].iov_len);
        }
    } else {
        for (int i = 0; i < iovcnt; i++) {
            ret += sos_sys_write(fildes, iov[i].iov_base, iov[i].iov_len);
        }
    }

    return ret;
}

long sys_readv(va_list ap)
{
    int fd = va_arg(ap, int);
    struct iovec *iov = va_arg(ap, struct iovec *);
    int iovcnt = va_arg(ap, int);
    int i;
    long read;

    read = 0;
    for (i = 0; i < iovcnt; i++) {
        read += sos_sys_read(fd, iov[i].iov_base, iov[i].iov_len);
    }
    return read;
}

long sys_read(va_list ap)
{
    int fd = va_arg(ap, int);
    void *buf = va_arg(ap, void *);
    size_t count = va_arg(ap, size_t);
    /* construct an iovec and call readv */
    struct iovec iov = {.iov_base = buf, .iov_len = count };
    return readv(fd, &iov, 1);
}

long sys_write(va_list ap)
{
    int fd = va_arg(ap, int);
    void *buf = va_arg(ap, void *);
    size_t count = va_arg(ap, size_t);
    /* construct an iovec and call readv */
    struct iovec iov = {.iov_base = buf, .iov_len = count };
    return writev(fd, &iov, 1);
}

long sys_ioctl(va_list ap)
{
    int fd = va_arg(ap, int);
    int request = va_arg(ap, int);
    (void)request;
    /* muslc does some ioctls to stdout, so just allow these to silently
       go through */
    if (fd == STDOUT_FD) {
        return 0;
    }
    assert(!"not implemented");
    return 0;
}

static long sos_sys_open_wrapper(const char *pathname, int flags)
{
    long fd = sos_sys_open(pathname, flags);
    if (fd == STDIN_FD || fd == STDOUT_FD || fd == STDERR_FD) {
        /* Internally muslc believes it is on a posix system with
         * stdin, stdout and stderr already open with fd's 0, 1 and 2
         * respectively. To make the system semi-sane we want to
         * allow muslc to keep using them so that such usages
         * can easily be detected. This means that your system
         * should not be returning these fd's as a result of
         * open calls otherwise things will get confusing. If you
         * do chose to have these fd's already open and existing
         * then you can remove this check. But make sure you
         * understand what is going on first! */
        assert(!"muslc is now going to be very confused");
        return -ENOMEM;
    }

    return fd;
}

long sys_openat(va_list ap)
{
    int dirfd = va_arg(ap, int);
    const char *pathname = va_arg(ap, const char *);
    int flags = va_arg(ap, int);
    UNUSED mode_t mode = va_arg(ap, mode_t);
    /* mask out flags we don't support */
    flags &= ~O_LARGEFILE;
    /* someone at some point got confused about what are flags and what the mode
     * is. so this does actually make sense */
    return sos_sys_open_wrapper(pathname, flags);
}

long sys_close(va_list ap)
{
    int fd = va_arg(ap, int);
    return sos_sys_close(fd);
}
