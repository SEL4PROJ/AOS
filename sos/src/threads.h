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

#include <sel4runtime.h>
#include <threads.h>
#include <cspace/cspace.h>
#include "ut.h"

extern cspace_t cspace;

typedef struct {
    ut_t *tcb_ut;
    seL4_CPtr tcb;

    seL4_CPtr user_ep;
    seL4_CPtr fault_ep;
    ut_t *ipc_buffer_ut;
    seL4_CPtr ipc_buffer;
    seL4_Word ipc_buffer_vaddr;

    ut_t *sched_context_ut;
    seL4_CPtr sched_context;

    ut_t *stack_ut;
    seL4_CPtr stack;
    seL4_Word badge;

    uintptr_t tls_base;
} sos_thread_t;

typedef void thread_main_f(void *);

extern __thread sos_thread_t *current_thread;

void init_threads(seL4_CPtr ep, seL4_CPtr sched_ctrl_start_, seL4_CPtr sched_ctrl_end_);
sos_thread_t *spawn(thread_main_f function, void *arg, seL4_Word badge, bool debugger_add);
sos_thread_t *debugger_spawn(thread_main_f function, void *arg, seL4_Word badge, seL4_CPtr bound_ntfn);
sos_thread_t *thread_create(thread_main_f function, void *arg, seL4_Word badge, bool resume, seL4_Word prio, 
                            seL4_CPtr bound_ntfn, bool debugger_add);
int thread_suspend(sos_thread_t *thread);
int thread_resume(sos_thread_t *thread);
