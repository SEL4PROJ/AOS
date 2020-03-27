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
#include <stdlib.h>
#include "utils.h"

#define CHANNEL_TYPE(name)                      name##_channel_t
#define CHANNEL_CREATE(name, ntfn)              name##_channel_create(ntfn)
#define CHANNEL_SEND(name, channel, message)    name##_channel_send(channel, message)
#define CHANNEL_RECV(name, channel)             name##_channel_recv(channel)
#define CHANNEL_IS_EMPTY(name, channel)         name##_channel_is_empty(channel)
#define CHANNEL(name, type, size) \
    typedef struct { \
        /* index of the next message in the buffer */ \
        size_t next_msg; \
        /* index of the next empty slot in the buffer */ \
        size_t next_empty; \
        /* Notification to indicate a message has been read (an additional shared notification) */ \
        seL4_CPtr read_ntfn; \
        /* Notification to indicate a message has been written (probably badged from the SOS bound notification) */ \
        seL4_CPtr write_ntfn; \
        type messages[size]; \
    } CHANNEL_TYPE(name); \
    \
    CHANNEL_TYPE(name) *name##_channel_create(seL4_CPtr read_available) { \
        CHANNEL_TYPE(name) *channel = malloc(sizeof(*channel)); \
        channel->next_msg = 0; \
        channel->next_empty = 0; \
        ut_t *ut = alloc_retype(&channel->read_ntfn, seL4_NotificationObject, seL4_NotificationBits); \
        if (ut == NULL) { \
            return NULL; \
        } \
        channel->write_ntfn = read_available; \
        return channel; \
    } \
    \
    void name##_channel_send(CHANNEL_TYPE(name) *channel, type message) { \
        /* Wait for an empty slot in the channel */ \
        while ((channel->next_empty + 1) % (size) == channel->next_msg) seL4_Wait(channel->read_ntfn, NULL); \
        /* Write the message into the channel */ \
        channel->messages[channel->next_empty] = message; \
        /* Update the write index */ \
        channel->next_empty += 1; \
        channel->next_empty %= size; \
        /* Signal any blocked receiver */ \
        seL4_Signal(channel->write_ntfn); \
    } \
    \
    type name##_channel_recv(CHANNEL_TYPE(name) *channel) { \
        /* assert at least one message */ \
        assert (channel->next_empty % (size) != channel->next_msg); \
        /* read the message from the channel */ \
        type message = channel->messages[channel->next_msg]; \
        /* Update the read index */ \
        channel->next_msg += 1; \
        channel->next_msg %= size; \
        /* Signal any blocked receiver */ \
        seL4_Signal(channel->read_ntfn); \
        return message; \
    } \
    \
    bool name##_channel_is_empty(CHANNEL_TYPE(name) *channel) { \
        return channel->next_empty % (size) == channel->next_msg % (size); \
    }
