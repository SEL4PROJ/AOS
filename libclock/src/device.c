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
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <clock/clock.h>
#include <utils/util.h>

#include "device.h"

typedef struct {
    seL4_Word irq;
    seL4_Word input_clk;
    seL4_Word enable;
    seL4_Word mode;
    size_t mux;
} timeout_info_t;

#define DEFINE_TIMEOUT(timeout) \
    [MESON_##timeout] = { \
        .irq = timeout##_IRQ, \
        .input_clk = timeout##_INPUT_CLK, \
        .enable = timeout##_EN, \
        .mode = timeout##_MODE, \
        .mux = timeout##_MUX, \
    }

static timeout_info_t timeouts[] = {
    DEFINE_TIMEOUT(TIMER_A),
    DEFINE_TIMEOUT(TIMER_B),
    DEFINE_TIMEOUT(TIMER_C),
    DEFINE_TIMEOUT(TIMER_D),
};

static uint32_t read_mux(volatile meson_timer_reg_t *regs, size_t mux);
static void write_mux(volatile meson_timer_reg_t *regs, size_t mux, uint32_t value);

void configure_timestamp(volatile meson_timer_reg_t *regs, timestamp_timebase_t timebase)
{
    uint32_t mux = regs->mux;
    mux &= ~(TIMESTAMP_TIMEBASE_MASK << TIMER_E_INPUT_CLK);
    mux |= timebase << TIMER_E_INPUT_CLK;
    regs->mux = mux;
    COMPILER_MEMORY_FENCE();
}

uint64_t read_timestamp(volatile meson_timer_reg_t *timer)
{
    COMPILER_MEMORY_FENCE();
    uint64_t lo = timer->timer_e;
    uint64_t hi = timer->timer_e_hi;
    COMPILER_MEMORY_FENCE();
    uint64_t new_lo = timer->timer_e;
    if (new_lo < lo) {
        lo = new_lo;
        hi = timer->timer_e_hi;
    }

    timestamp_t time = hi << 32;
    time |= lo & MASK(32);
    return time;
}

void configure_timeout(volatile meson_timer_reg_t *regs, timeout_id_t timer, bool enable, bool periodic,
                       timeout_timebase_t timebase, uint16_t timeout)
{
    assert(timer < ARRAY_SIZE(timeouts));
    timeout_info_t info = timeouts[timer];

    /* Disable the timer */
    uint32_t mux = read_mux(regs, info.mux);
    mux &= ~info.enable;
    write_mux(regs, info.mux, mux);

    if (!enable) {
        /* If not enabling the timer, just diable and exit */
        return;
    }

    /* Configure the timeout */
    write_timeout(regs, timer, timeout);

    /* Configure the timer */
    mux = read_mux(regs, info.mux);
    mux |= info.enable;

    if (periodic) {
        mux |= info.mode;
    } else {
        mux &= ~info.mode;
    }

    /* Clear the existing clock rate */
    mux &= ~(TIMEOUT_TIMEBASE_MASK << info.input_clk);

    /* Set the new clock rate */
    mux |= timebase << info.input_clk;

    write_mux(regs, info.mux, mux);
}

uint16_t read_timeout(volatile meson_timer_reg_t *regs, timeout_id_t timer)
{
    assert(timer < ARRAY_SIZE(timeouts));

    COMPILER_MEMORY_FENCE();

    uint32_t value;
    switch (timer) {
    case MESON_TIMER_A:
        value = regs->timer_a;
        break;
    case MESON_TIMER_B:
        value = regs->timer_b;
        break;
    case MESON_TIMER_C:
        value = regs->timer_c;
        break;
    case MESON_TIMER_D:
        value = regs->timer_d;
        break;
    }

    return value >> 16;
}

void write_timeout(volatile meson_timer_reg_t *regs, timeout_id_t timer, uint16_t value)
{
    switch (timer) {
    case MESON_TIMER_A:
        regs->timer_a = value;
        break;
    case MESON_TIMER_B:
        regs->timer_b = value;
        break;
    case MESON_TIMER_C:
        regs->timer_c = value;
        break;
    case MESON_TIMER_D:
        regs->timer_d = value;
        break;
    }
    COMPILER_MEMORY_FENCE();
}

seL4_Word meson_timeout_irq(timeout_id_t timer)
{
    assert(timer < ARRAY_SIZE(timeouts));
    return timeouts[timer].irq;
}

static uint32_t read_mux(volatile meson_timer_reg_t *regs, size_t mux)
{
    COMPILER_MEMORY_FENCE();
    switch (mux) {
    case 0:
        return regs->mux;
    case 1:
        return regs->mux1;
    }
}

static void write_mux(volatile meson_timer_reg_t *regs, size_t mux, uint32_t value)
{
    switch (mux) {
    case 0:
        regs->mux = value;
        break;
    case 1:
        regs->mux1 = value;
        break;
    }
    COMPILER_MEMORY_FENCE();
}
