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
#include <stdint.h>
#include <ctype.h>

#include "unimplemented.h"

uint64_t uboot_timestamp_freq = 0;

void uboot_timer_init()
{
    uboot_timestamp_freq = timestamp_get_freq();
}

unsigned long simple_strtoul(const char *cp, char **endp,
                             unsigned int base)
{
    unsigned long result = 0;
    unsigned long value;

    if (*cp == '0') {
        cp++;
        if ((*cp == 'x') && isxdigit(cp[1])) {
            base = 16;
            cp++;
        }

        if (!base) {
            base = 8;
        }
    }

    if (!base) {
        base = 10;
    }

    while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp - '0' : (islower(*cp)
                                                                 ? toupper(*cp) : *cp) - 'A' + 10) < base) {
        result = result * base + value;
        cp++;
    }

    if (endp) {
        *endp = (char *)cp;
    }

    return result;
}

