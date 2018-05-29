/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
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

#include <sel4/sel4.h>

#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <sys/types.h>
#include <sys/syscall.h>

#define STDOUT_FD 1
#define STDERR_FD 2

static size_t sel4_write(void *data, size_t count) {
#ifdef SEL4_DEBUG_KERNEL
    size_t i;
    char *realdata = data;
    for (i = 0; i < count; i++) {
        seL4_DebugPutChar(realdata[i]);
    }
#else
    (void)data;
#endif
    return count;
}

long
sys_writev(va_list ap)
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
            ret += sel4_write(iov[i].iov_base, iov[i].iov_len);
        }
    } else {
        assert(!"Not implemented");
        return -EBADF;
    }

    return ret;
}

long sys_readv(va_list ap)
{
    /* rootserver cannot read input */
    assert(!"not implemented");
    return 0;
}

long sys_read(va_list ap)
{
    int fd = va_arg(ap, int);
    void *buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);
    /* construct an iovec and call readv */
    struct iovec iov = {.iov_base = buf, .iov_len = count };
    return readv(fd, &iov, 1);
}

long
sys_ioctl(va_list ap)
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

long
sys_open(va_list ap)
{
    assert(!"not implemented");
    return 0;
}

long
sys_close(va_list ap)
{
    assert(!"not implemented");
    return 0;
}
