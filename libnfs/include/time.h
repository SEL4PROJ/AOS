/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef __NFS_TIME_H
#define __NFS_TIME_H

#include <stdint.h>
#include <lwip/ip_addr.h>


/**
 * Retrieve the time using the udp time protocol
 * @param server the ip address of the time server
 * @return seconds past the UTC 1900 epoch or 0 on failure
 */
uint32_t udp_time_get(const struct ip_addr *server);



#endif /* __NFS_TIME_H */
