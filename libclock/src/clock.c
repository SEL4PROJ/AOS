/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include <clock/clock.h>


int start_timer(seL4_CPtr interrupt_ntfn)
{
    return CLOCK_R_FAIL;
}

uint32_t register_timer(uint64_t delay, timer_callback_t callback, void *data)
{
    return 0;
}

int remove_timer(uint32_t id)
{
    return CLOCK_R_FAIL;
}

int timer_interrupt(void)
{
    return CLOCK_R_FAIL;
}

timestamp_t time_stamp(void)
{
    return 0;
}

int stop_timer(void)
{
    return CLOCK_R_FAIL;
}
