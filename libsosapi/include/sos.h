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
/* Simple operating system interface */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sel4/sel4.h>

/* System calls for SOS */

/* Endpoint for talking to SOS */
#define SOS_IPC_EP_CAP     (0x1)
#define TIMER_IPC_EP_CAP   (0x2)

/* Limits */
#define PROCESS_MAX_FILES 16
#define MAX_IO_BUF 0x1000
#define N_NAME 32

/* file modes */
#define FM_EXEC  1
#define FM_WRITE 2
#define FM_READ  4
typedef int fmode_t;

/* stat file types */
#define ST_FILE    1    /* plain file */
#define ST_SPECIAL 2    /* special (console) file */
typedef int st_type_t;


typedef struct {
    st_type_t st_type;    /* file type */
    fmode_t   st_fmode;   /* access mode */
    unsigned  st_size;    /* file size in bytes */
    long      st_ctime;   /* Unix file creation time (ms) */
    long      st_atime;   /* Unix file last access (open) time (ms) */
} sos_stat_t;

typedef int pid_t;

typedef struct {
    pid_t     pid;
    unsigned  size;            /* in pages */
    unsigned  stime;           /* start time in msec since booting */
    char      command[N_NAME]; /* Name of exectuable */
} sos_process_t;

/* I/O system calls */

int sos_sys_open(const char *path, fmode_t mode);
/* Open file and return file descriptor, -1 if unsuccessful
 * (too many open files, console already open for reading).
 * A new file should be created if 'path' does not already exist.
 * A failed attempt to open the console for reading (because it is already
 * open) will result in a context switch to reduce the cost of busy waiting
 * for the console.
 * "path" is file name, "mode" is one of O_RDONLY, O_WRONLY, O_RDWR.
 */

int sos_sys_close(int file);
/* Closes an open file. Returns 0 if successful, -1 if not (invalid "file").
 */

int sos_sys_read(int file, char *buf, size_t nbyte);
/* Read from an open file, into "buf", max "nbyte" bytes.
 * Returns the number of bytes read.
 * Will block when reading from console and no input is presently
 * available. Returns -1 on error (invalid file).
 */

int sos_sys_write(int file, const char *buf, size_t nbyte);
/* Write to an open file, from "buf", max "nbyte" bytes.
 * Returns the number of bytes written. <nbyte disk is full.
 * Returns -1 on error (invalid file).
 */

int sos_getdirent(int pos, char *name, size_t nbyte);
/* Reads name of entry "pos" in directory into "name", max "nbyte" bytes.
 * Returns number of bytes returned, zero if "pos" is next free entry,
 * -1 if error (non-existent entry).
 */

int sos_stat(const char *path, sos_stat_t *buf);
/* Returns information about file "path" through "buf".
 * Returns 0 if successful, -1 otherwise (invalid name).
 */

pid_t sos_process_create(const char *path);
/* Create a new process running the executable image "path".
 * Returns ID of new process, -1 if error (non-executable image, nonexisting
 * file).
 */

int sos_process_delete(pid_t pid);
/* Delete process (and close all its file descriptors).
 * Returns 0 if successful, -1 otherwise (invalid process).
 */

pid_t sos_my_id(void);
/* Returns ID of caller's process. */

int sos_process_status(sos_process_t *processes, unsigned max);
/* Returns through "processes" status of active processes (at most "max"),
 * returns number of process descriptors actually returned.
 */

pid_t sos_process_wait(pid_t pid);
/* Wait for process "pid" to exit. If "pid" is -1, wait for any process
 * to exit. Returns the pid of the process which exited.
 */

int64_t sos_sys_time_stamp(void);
/* Returns time in microseconds since booting.
 */

void sos_sys_usleep(int msec);
/* Sleeps for the specified number of milliseconds.
 */


/*************************************************************************/
/*                                   */
/* Optional (bonus) system calls                     */
/*                                   */
/*************************************************************************/

int sos_share_vm(void *adr, size_t size, int writable);
/* Make VM region ["adr","adr"+"size") sharable by other processes.
 * If "writable" is non-zero, other processes may have write access to the
 * shared region. Both, "adr" and "size" must be divisible by the page size.
 *
 * In order for a page to be shared, all participating processes must execute
 * the system call specifying an interval including that page.
 * Once a page is shared, a process may write to it if and only if all
 * _other_ processes have set up the page as shared writable.
 *
 * Returns 0 if successful, -1 otherwise (invalid address or size).
 */
