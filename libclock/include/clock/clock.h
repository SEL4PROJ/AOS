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

#include <stdint.h>
#include <sel4/sel4.h>
#include <clock/device.h>
#include <clock/timestamp.h>

/*
 * Return codes for driver functions
 */
#define CLOCK_R_OK     0        /* success */
#define CLOCK_R_UINT (-1)       /* driver not initialised */
#define CLOCK_R_CNCL (-2)       /* operation cancelled (driver stopped) */
#define CLOCK_R_FAIL (-3)       /* operation failed for other reason */

typedef uint64_t timestamp_t;
typedef void (*timer_callback_t)(uint32_t id, void *data);


/*
 * Initialise driver. Performs implicit stop_timer() if already initialised.
 *
 * @param timer_vaddr  Virtual address of mapped page containing device
 *                     registers
 * @return             CLOCK_R_OK iff successful.
 */
int start_timer(unsigned char *timer_vaddr);

/**
 * Get the current clock time in microseconds.
 */
timestamp_t get_time(void);

/**
 * Register a callback to be called after a given delay
 *
 * @param delay     Delay time in microseconds before callback is invoked
 * @param callback  Function to be called
 * @param data      Custom data to be passed to callback function
 * @return          0 on failure, otherwise an unique ID for this timeout
 */
uint32_t register_timer(uint64_t delay, timer_callback_t callback, void *data);

/**
 * Remove a previously registered callback by its ID
 *
 * @param id  Unique ID returned by register_time
 * @return    CLOCK_R_OK iff successful.
 */
int remove_timer(uint32_t id);

/*
 * Stop clock driver operation.
 *
 * @return   CLOCK_R_OK iff successful.
 */
int stop_timer(void);

/*
 * Signal to the timer than an IRQ was received.
 */
int timer_irq(
    void *data,
    seL4_Word irq,
    seL4_IRQHandler irq_handler
);
