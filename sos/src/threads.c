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
#include "threads.h"

#include <stdlib.h>
#include <utils/util.h>
#include <sel4runtime.h>
#include <aos/debug.h>
#include <cspace/cspace.h>

#include "ut.h"
#include "vmem_layout.h"
#include "utils.h"
#include "mapping.h"

#define SOS_THREAD_PRIORITY     (0)

__thread sos_thread_t *current_thread = NULL;

static seL4_CPtr sched_ctrl_start;
static seL4_CPtr sched_ctrl_end;

static seL4_CPtr ipc_ep;

void init_threads(seL4_CPtr ep, seL4_CPtr sched_ctrl_start_, seL4_CPtr sched_ctrl_end_)
{
    ipc_ep = ep;
    sched_ctrl_start = sched_ctrl_start_;
    sched_ctrl_end = sched_ctrl_end_;
}


/* leaking a lot of memory if failed! */
static bool alloc_stack(seL4_Word *sp)
{
    static seL4_Word curr_stack = SOS_STACK + SOS_STACK_PAGES * PAGE_SIZE_4K;
    // Skip guard page
    curr_stack += PAGE_SIZE_4K;
    for (int i = 0; i < SOS_STACK_PAGES; i++) {
        seL4_CPtr frame_cap;
        ut_t *frame = alloc_retype(&frame_cap, seL4_ARM_SmallPageObject, seL4_PageBits);
        if (frame == NULL) {
            ZF_LOGE("Failed to allocate stack page");
            return false;
        }
        seL4_Error err = map_frame(&cspace, frame_cap, seL4_CapInitThreadVSpace,
                                   curr_stack, seL4_AllRights, seL4_ARM_Default_VMAttributes);
        if (err != seL4_NoError) {
            ZF_LOGE("Failed to map stack");
            return false;
        }
        curr_stack += PAGE_SIZE_4K;
    }
    *sp = curr_stack;
    return true;
}

int thread_suspend(sos_thread_t *thread)
{
    return seL4_TCB_Suspend(thread->tcb);
}

int thread_resume(sos_thread_t *thread)
{
    return seL4_TCB_Resume(thread->tcb);
}

/* trampoline code for newly started thread */
static void thread_trampoline(sos_thread_t *thread, thread_main_f *function, void *arg)
{
    sel4runtime_set_tls_base(thread->tls_base);
    seL4_SetIPCBuffer((seL4_IPCBuffer *) thread->ipc_buffer_vaddr);
    current_thread = thread;
    function(arg);
    thread_suspend(thread);
}

/*
 * Spawn a new kernel (SOS) thread to execute function with arg
 *
 * TODO: fix memory leaks
 */
sos_thread_t *thread_create(thread_main_f function, void *arg, seL4_Word badge, bool resume)
{
    /* we allocate stack for additional sos threads
     * on top of the stack for sos */
    static seL4_Word curr_ipc_buf = SOS_IPC_BUFFER;

    sos_thread_t *new_thread = malloc(sizeof(*new_thread));
    if (new_thread == NULL) {
        return NULL;
    }

    new_thread->badge = badge;

    /* Create an IPC buffer */
    new_thread->ipc_buffer_ut = alloc_retype(&new_thread->ipc_buffer,
                                             seL4_ARM_SmallPageObject, seL4_PageBits);
    if (new_thread->ipc_buffer_ut == NULL) {
        ZF_LOGE("Failed to alloc ipc buffer ut");
        return NULL;
    }

    /* Set up TLS for the new thread */
    void *tls_memory = malloc(sel4runtime_get_tls_size());
    if (tls_memory == NULL) {
        ZF_LOGE("Failed to alloc memory for tls");
        return NULL;
    }
    new_thread->tls_base = sel4runtime_write_tls_image(tls_memory);
    if (new_thread->tls_base == (uintptr_t) NULL) {
        ZF_LOGE("Failed to write tls image");
        return NULL;
    }

    /* allocate a new slot in the target cspace which we will mint a badged endpoint cap into --
     * the badge is used to identify the process, which will come in handy when you have multiple
     * processes. */
    new_thread->user_ep = cspace_alloc_slot(&cspace);
    if (new_thread->user_ep == seL4_CapNull) {
        ZF_LOGE("Failed to alloc user ep slot");
        return NULL;
    }

    /* now mutate the cap, thereby setting the badge */
    seL4_Word err = cspace_mint(&cspace, new_thread->user_ep, &cspace, ipc_ep, seL4_AllRights,
                                badge);
    if (err) {
        ZF_LOGE("Failed to mint user ep");
        return NULL;
    }

    /* Create a new TCB object */
    new_thread->tcb_ut = alloc_retype(&new_thread->tcb, seL4_TCBObject, seL4_TCBBits);
    if (new_thread->tcb_ut == NULL) {
        ZF_LOGE("Failed to alloc tcb ut");
        return NULL;
    }

    /* Configure the TCB */
    err = seL4_TCB_Configure(new_thread->tcb,
                             cspace.root_cnode, seL4_NilData,
                             seL4_CapInitThreadVSpace, seL4_NilData, curr_ipc_buf,
                             new_thread->ipc_buffer);
    if (err != seL4_NoError) {
        ZF_LOGE("Unable to configure new TCB");
        return NULL;
    }

    /* Create scheduling context */
    new_thread->sched_context_ut = alloc_retype(&new_thread->sched_context,
                                                seL4_SchedContextObject,
                                                seL4_MinSchedContextBits);
    if (new_thread->sched_context_ut == NULL) {
        ZF_LOGE("Failed to alloc sched context ut");
        return NULL;
    }

    /* Configure the scheduling context to use the second core with budget equal to period */
    seL4_CPtr sched_ctrl;
    if (sched_ctrl_start + 1 < sched_ctrl_end) {
        sched_ctrl = sched_ctrl_start + 1;
    } else {
        sched_ctrl = sched_ctrl_start;
    }
    err = seL4_SchedControl_Configure(sched_ctrl, new_thread->sched_context,
                                      US_IN_MS, US_IN_MS, 0, 0);
    if (err != seL4_NoError) {
        ZF_LOGE("Unable to configure scheduling context");
        return NULL;
    }

    /* bind sched context, set fault endpoint and priority
     * In MCS, fault end point needed here should be in current thread's cspace.
     * NOTE this will use the unbadged ep unlike above, you might want to mint it with a badge
     * so you can identify which thread faulted in your fault handler */
    err = seL4_TCB_SetSchedParams(new_thread->tcb, seL4_CapInitThreadTCB, seL4_MinPrio,
                                  SOS_THREAD_PRIORITY, new_thread->sched_context,
                                  new_thread->user_ep);
    if (err != seL4_NoError) {
        ZF_LOGE("Unable to set scheduling params");
        return NULL;
    }

    /* Provide a name for the thread -- Helpful for debugging */
    NAME_THREAD(new_thread->tcb, "second sos thread");

    /* set up the stack */
    seL4_Word sp;
    if (!alloc_stack(&sp)) {
        return NULL;
    }

    /* Map in the IPC buffer for the thread */
    err = map_frame(&cspace, new_thread->ipc_buffer, seL4_CapInitThreadVSpace, curr_ipc_buf,
                    seL4_AllRights, seL4_ARM_Default_VMAttributes);
    if (err != 0) {
        ZF_LOGE("Unable to map IPC buffer for user app");
        return NULL;
    }
    new_thread->ipc_buffer_vaddr = curr_ipc_buf;
    curr_ipc_buf += PAGE_SIZE_4K;

    /* set initial context */
    seL4_UserContext context = {
        .pc = (seL4_Word) thread_trampoline,
        .sp = sp,
        .x0 = (seL4_Word) new_thread,
        .x1 = (seL4_Word) function,
        .x2 = (seL4_Word) arg,
    };
    ZF_LOGD(resume ? "Starting new sos thread at %p\n"
            : "Created new thread starting at %p\n", (void *) context.pc);
    fflush(NULL);
    err = seL4_TCB_WriteRegisters(new_thread->tcb, resume, 0, 6, &context);
    if (err != seL4_NoError) {
        ZF_LOGE("Failed to write registers");
        return NULL;
    }
    return new_thread;
}

sos_thread_t *spawn(thread_main_f function, void *arg, seL4_Word badge)
{
    return thread_create(function, arg, badge, true);
}
