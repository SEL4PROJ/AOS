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
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <cspace/cspace.h>
#include <cspace/bitfield.h>
#include <sel4/sel4.h>
#include <utils/util.h>
#include <utils/zf_log.h>

#include "irq.h"

typedef struct {
    seL4_Word irq;
    seL4_IRQHandler irq_handler;
    seL4_CPtr notification;
    sos_irq_callback_t callback;
    void *data;
} irq_handler_t;

static irq_handler_t irq_handlers[seL4_BadgeBits];

static struct {
    seL4_IRQControl irq_control;
    seL4_CPtr notification;
    cspace_t *cspace;
    seL4_Word flag_bits;
    seL4_Word ident_bits;
    seL4_Word allocated_bits;
} irq_dispatch;

/*
 * Allocate a bit to set to uniquely identify an IRQ in a badge.
 */
static unsigned long alloc_irq_bit(void);
static void free_irq_bit(unsigned long bit);
static int dispatch_irq(irq_handler_t *irq_handler);

void sos_init_irq_dispatch(
    cspace_t *cspace,
    seL4_IRQControl irq_control,
    seL4_CPtr notification,
    seL4_Word flag_bits,
    seL4_Word ident_bits
)
{
    /* Flag bits and identifier bits must not overlap */
    assert((flag_bits & ident_bits) == 0);

    irq_dispatch.irq_control = irq_control;
    irq_dispatch.notification = notification;
    irq_dispatch.cspace = cspace;
    irq_dispatch.flag_bits = flag_bits;
    irq_dispatch.ident_bits = ident_bits;
    irq_dispatch.allocated_bits = ~ident_bits;
}

int sos_register_irq_handler(
    seL4_Word irq,
    bool edge_triggered,
    sos_irq_callback_t callback,
    void *data,
    seL4_IRQHandler *irq_handler
)
{
    unsigned long ident_bit = alloc_irq_bit();
    if (ident_bit >= seL4_BadgeBits) {
        ZF_LOGE("Exhausted IRQ notificatoin bits for IRQ #%lu", irq);
        return ENOMEM;
    }

    seL4_CPtr handler_cptr = cspace_alloc_slot(irq_dispatch.cspace);
    if (handler_cptr == 0) {
        ZF_LOGE("Could not allocate irq handler slot for IRQ #%lu", irq);
        free_irq_bit(ident_bit);
        return ENOMEM;
    }

    seL4_CPtr notification_cptr = cspace_alloc_slot(irq_dispatch.cspace);
    if (handler_cptr == 0) {
        ZF_LOGE("Could not allocate notification slot for IRQ #%lu", irq);
        cspace_free_slot(irq_dispatch.cspace, handler_cptr);
        free_irq_bit(ident_bit);
        return ENOMEM;
    }

    seL4_Error err = cspace_irq_control_get(irq_dispatch.cspace, handler_cptr, irq_dispatch.irq_control, irq,
                                            edge_triggered);
    if (err != 0) {
        ZF_LOGE("Could not allocate irq handler for IRQ #%lu", irq);
        cspace_free_slot(irq_dispatch.cspace, handler_cptr);
        cspace_free_slot(irq_dispatch.cspace, notification_cptr);
        free_irq_bit(ident_bit);
        return err;
    }

    seL4_Word badge = irq_dispatch.flag_bits | BIT(ident_bit);

    err = cspace_mint(irq_dispatch.cspace, notification_cptr, irq_dispatch.cspace, irq_dispatch.notification, seL4_CanWrite,
                      badge);
    if (err != 0) {
        ZF_LOGE("Could not mint notification for IRQ #%lu", irq);
        cspace_delete(irq_dispatch.cspace, handler_cptr);
        cspace_free_slot(irq_dispatch.cspace, handler_cptr);
        cspace_free_slot(irq_dispatch.cspace, notification_cptr);
        free_irq_bit(ident_bit);
        return err;
    }

    err = seL4_IRQHandler_SetNotification(handler_cptr, notification_cptr);
    if (err != 0) {
        ZF_LOGE("Could not set notification for IRQ #%lu", irq);
        cspace_delete(irq_dispatch.cspace, notification_cptr);
        cspace_delete(irq_dispatch.cspace, handler_cptr);
        cspace_free_slot(irq_dispatch.cspace, handler_cptr);
        cspace_free_slot(irq_dispatch.cspace, notification_cptr);
        free_irq_bit(ident_bit);
        return err;
    }


    irq_handlers[ident_bit] = (irq_handler_t) {
        .irq = irq,
        .irq_handler = handler_cptr,
        .notification = notification_cptr,
        .callback = callback,
        .data = data,
    };

    if (irq_handler != NULL) {
        *irq_handler = handler_cptr;
    }

    ZF_LOGI("Registered IRQ #%lu with badge 0x%lX", irq, badge);
    return 0;
}

int sos_handle_irq_notification(seL4_Word *badge)
{
    unsigned long unchecked_bits =
        *badge &
        irq_dispatch.allocated_bits &
        irq_dispatch.ident_bits;

    while (unchecked_bits) {
        unsigned long bit = CTZL(unchecked_bits);
        irq_handler_t *irq_handler = &irq_handlers[bit];
        ZF_LOGD("Handling IRQ #%lu", irq_handler->irq);

        /* Any bits that have been set that we have allocated*/
        int err = dispatch_irq(irq_handler);
        if (err != 0) {
            ZF_LOGE("Error handling IRQ #%lu", irq_handler->irq);
            return err;
        }

        /* Unset the bit that was handled. */
        *badge &= ~BIT(bit);
        unchecked_bits &= *badge;
    };

    return 0;
}

static int dispatch_irq(irq_handler_t *irq_handler)
{
    if (irq_handler->callback != NULL) {
        return irq_handler->callback(irq_handler->data, irq_handler->irq, irq_handler->irq_handler);
    } else {
        return -1;
    }
}

static unsigned long alloc_irq_bit(void)
{
    unsigned long bit = bf_first_free(1, &irq_dispatch.allocated_bits);
    irq_dispatch.allocated_bits |= BIT(bit);
    return bit;
}

static void free_irq_bit(unsigned long bit)
{
    irq_dispatch.allocated_bits &= ~BIT(bit);
}
