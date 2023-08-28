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
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sos.h>

#include <sel4/sel4.h>

static size_t sos_debug_print(const void *vData, size_t count)
{
#ifdef CONFIG_DEBUG_BUILD
    size_t i;
    const char *realdata = vData;
    for (i = 0; i < count; i++) {
        seL4_DebugPutChar(realdata[i]);
    }
#endif
    return count;
}

int sos_open(const char *path, fmode_t mode)
{
    assert(!"You need to implement this");
    return -1;
}

int sos_close(int file)
{
    assert(!"You need to implement this");
    return -1;
}

int sos_read(int file, char *buf, size_t nbyte)
{
    assert(!"You need to implement this");
    return -1;
}

int sos_write(int file, const char *buf, size_t nbyte)
{
    /* MILESTONE 0: implement this to use your syscall and
     * writes to the network console!
     * Writing to files will come in later milestones.
     */
    return sos_debug_print(buf, nbyte);
}

int sos_getdirent(int pos, char *name, size_t nbyte)
{
    assert(!"You need to implement this");
    return -1;
}

int sos_stat(const char *path, sos_stat_t *buf)
{
    assert(!"You need to implement this");
    return -1;
}

pid_t sos_process_create(const char *path)
{
    assert(!"You need to implement this");
    return -1;
}

int sos_process_delete(pid_t pid)
{
    assert(!"You need to implement this");
    return -1;
}

pid_t sos_my_id(void)
{
    assert(!"You need to implement this");
    return -1;

}

int sos_process_status(sos_process_t *processes, unsigned max)
{
    assert(!"You need to implement this");
    return -1;
}

pid_t sos_process_wait(pid_t pid)
{
    assert(!"You need to implement this");
    return -1;

}

void sos_usleep(int msec)
{
    assert(!"You need to implement this");
}

int64_t sos_time_stamp(void)
{
    assert(!"You need to implement this");
    return -1;
}
