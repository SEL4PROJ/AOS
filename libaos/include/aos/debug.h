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
#pragma once

#include <autoconf.h>

#include <inttypes.h>
#include <sel4/sel4.h>
#include <sel4/types.h>
#include <aos/registers.h>

#ifdef CONFIG_DEBUG_BUILD
#define NAME_THREAD(_tcbcap, _name)   seL4_DebugNameThread(_tcbcap, _name);
#else
#define NAME_THREAD(_tcbcap, _name)
#endif

void debug_cap_identify(seL4_CPtr cap);
void debug_print_bootinfo(seL4_BootInfo *info);
void debug_print_fault(seL4_MessageInfo_t fault, const char *thread_name);
void debug_dump_registers(seL4_CPtr tcb);

