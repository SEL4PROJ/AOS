/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef __PBUF_HELPERS_H
#define __PBUF_HELPERS_H

#include <stdint.h>
#include <lwip/pbuf.h>

/* Align pos to the start of a network long */
void pb_alignl(int* pos);

/* write raw data to the pbuf, update pos and return 0 on success  */
void pb_write(struct pbuf* pbuf, const void* data, int len, int* pos);
/* read raw data from the pbuf, update pos and return 0 on success */
void pb_read(struct pbuf* pbuf, void* data, int len, int* pos);

/* Read a string from the pbuf, update pos and return 0 on success */
void pb_read_str(struct pbuf* pbuf, char* str, int maxlen, int* pos);
/* Write a string to the pbuf, update pos and return 0 on success */
void pb_write_str(struct pbuf* pbuf, const char* str, uint32_t len, int* pos);

/* Write a single host order long into the buf in network order.
 * return 0 on success */
void pb_writel(struct pbuf* pbuf, uint32_t v, int* pos);
/* read a single host order long from the buf in network order.
 * return 0 on success */
void pb_readl(struct pbuf* pbuf, uint32_t *v, int* pos);

/* Write an array of host order longs into the buf in network order.
 * Size is the size of the array in bytes.
 * Update pos and return 0 on success */
void pb_write_arrl(struct pbuf* pbuf, const uint32_t* arr, int size, int* pos);
/* Read an array of network order longs into the buf in host order.
 * Size is the size of the array in bytes.
 * Update pos and return 0 on success */
void pb_read_arrl(struct pbuf* pbuf, uint32_t* arr, int size, int* pos);


#endif /* __PBUF_HELPERS_H */
