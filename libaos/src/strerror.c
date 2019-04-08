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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <aos/strerror.h>

char *sel4_errlist[] = {
    [seL4_NoError] = "seL4_NoError",
    [seL4_InvalidArgument] = "seL4_InvalidArgument",
    [seL4_InvalidCapability] = "seL4_InvalidCapability",
    [seL4_IllegalOperation] = "seL4_IllegalOperation",
    [seL4_RangeError] = "seL4_RangeError",
    [seL4_AlignmentError] = "seL4_AlignmentError",
    [seL4_FailedLookup] = "seL4_FailedLookup",
    [seL4_TruncatedMessage] = "seL4_TruncatedMessage",
    [seL4_DeleteFirst] = "seL4_DeleteFirst",
    [seL4_RevokeFirst] = "seL4_RevokeFirst",
    [seL4_NotEnoughMemory] = "seL4_NotEnoughMemory",
    NULL
};

const char *sel4_strerror(int errcode)
{
    assert(errcode < sizeof(sel4_errlist) / sizeof(*sel4_errlist) - 1);
    return sel4_errlist[errcode];
}

void __sel4_error(int sel4_error, const char *file,
                  const char *function, int line, char *str)
{
    printf("seL4 Error: %s, function %s, file %s, line %d: %s\n",
           sel4_errlist[sel4_error],
           function, file, line, str);
    abort();
}
