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
/*
 * Asynchronous IRQ handling and dispatch.
 */

#include <stdbool.h>
#include <sel4/sel4.h>

#pragma once

/*
 * An IRQ handler captures its own state and can be invoked for IRQs
 * that it needs to handle.
 */
typedef int (*sos_irq_callback_t)(
    void *data,
    seL4_Word irq,
    seL4_IRQHandler irq_handler
);

/*
 * Initialise the IRQ dispatch system.
 *
 * @cspace       The cspace for the sos root task
 * @irq_control  CPtr for the IRQ control capability
 * @notification CPtr for the notification to which IRQs should be
 *               attached.
 * @flag_bits    Bits to set on badges of minted notifications to
 *               indicate that a notification rather than an IPC was
 *               sent.
 * @ident_bits   Bits that can be set in minted notifications to
 *               uniquely identify IRQs.
 */
void sos_init_irq_dispatch(
    cspace_t *cspace,
    seL4_IRQControl irq_control,
    seL4_CPtr notification,
    seL4_Word flag_bits,
    seL4_Word ident_bits
);

/*
 * Register a new IRQ handler.
 *
 * @irq             The irq number for which a handler should be
 *                  registered.
 * @edge_triggered  Whether or not the IRQ is edge triggered.
 * @callback        Callback to trigger for the IRQ.
 * @data            Data to pass to the callback.
 * @irq_handler     Pointer to a CPtr for an IRQHandler. This will be
 *                  set to the CPtr for the handler to be used to
 *                  acknowledge the IRQ (this is the same handler passed
 *                  to the registered callback).
 */
int sos_register_irq_handler(
    seL4_Word irq,
    bool edge_triggered,
    sos_irq_callback_t callback,
    void *data,
    seL4_IRQHandler *irq_handler
);

/*
 * Handle all IRQs triggered by a notification.
 *
 * Will check all bits of the badge and call registered callbacks for
 * any associated IRQs, unsetting them in the badge when they have been
 * handled.
 *
 * Returns any errors raised during handling IRQs or 0 on success.
 */
int sos_handle_irq_notification(seL4_Word *badge);
