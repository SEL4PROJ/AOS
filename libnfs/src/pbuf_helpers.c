/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "pbuf_helpers.h"
#include <lwip/def.h>

#include <string.h>
#include <assert.h>

#define MIN(a,b) (((a)<(b))? (a) : (b))
#define ROUNDDOWN(v, r) ((v) - ((v) & ((r) - 1)))
#define ROUNDUP(v, r)   ROUNDDOWN(v + (r) - 1, r)

//#define DEBUG_PBUF
#ifdef DEBUG_PBUF
#define debug(x...) printf( x )
#else
#define debug(x...)
#endif

/* Align pos to the start of a network long */
void pb_alignl(int* pos)
{
    *pos = ROUNDUP(*pos, sizeof(uint32_t));
}


/* LWIP should really provide this for us... */
static u16_t 
pbuf_take_partial(struct pbuf *p, const void *dataptr, u16_t len, u16_t offset)
{
    int written = 0;
    if(!len){
        return 0;
    }
    assert(offset < p->tot_len);
    assert(offset + len <= p->tot_len);
    /* seek */
    while(offset >= p->len){
        offset -= p->len;
        p = p->next;
    }
    /* write */
    while(len > written){
        int bytes = MIN(p->len - offset, len - written);
        memcpy((char*)p->payload + offset, dataptr, bytes);
        p = p->next;
        written += bytes;
        offset += bytes;
        dataptr += bytes;
        offset = 0;
    }
    return len;
}

void
pb_write(struct pbuf* pbuf, const void* data, int count, int* pos)
{
    *pos = *pos + pbuf_take_partial(pbuf, data, count, *pos);
}

void
pb_write_arrl(struct pbuf* pbuf, const uint32_t* arr, int size, int* pos)
{
    uint32_t nl_arr[size/sizeof(uint32_t)];
    int i;
    for(i = 0; i < size/sizeof(uint32_t); i++){
        nl_arr[i] = htonl(arr[i]);
    }
    pb_write(pbuf, nl_arr, sizeof(nl_arr), pos);
}

/* Write a single host order long into the buf in network order.
 * return 0 on success */
void
pb_writel(struct pbuf* pbuf, uint32_t v, int* pos)
{
    pb_write_arrl(pbuf, &v, sizeof(v), pos);
}

/* Read a string from the pbuf, update pos and return 0 on success */
void
pb_write_str(struct pbuf* pbuf, const char* str, uint32_t len, int* pos)
{
    uint32_t padding = 0;
    /* Write the string length and string */
    pb_write_arrl(pbuf, &len, sizeof(len), pos);
    pb_write(pbuf, str, len, pos);
    /* Write the padding */
    pb_write(pbuf, &padding, ROUNDUP(*pos, 4) - *pos, pos);
}


/* 
 * NOTE: we allow 0 count reads to transparently handle the case where we are 
 *       reading in data of size 0
 */
void
pb_read(struct pbuf* pbuf, void* data, int count, int* pos)
{
    u16_t read;
    assert(*pos <= pbuf->tot_len);
    assert(*pos + count <= pbuf->tot_len);
    read = pbuf_copy_partial(pbuf, data, count, *pos);
    assert(count == read);
    *pos = *pos + read;
}

void
pb_read_arrl(struct pbuf* pbuf, uint32_t* arr, int size, int* pos)
{
    int i;
    pb_read(pbuf, arr, size, pos);
    for(i = 0; i < size/sizeof(*arr); i++){
        arr[i] = ntohl(arr[i]);
    }
}

/* read a single host order long from the buf in network order.
 * return 0 on success */
void
pb_readl(struct pbuf* pbuf, uint32_t *v, int* pos)
{
    pb_read_arrl(pbuf, v, sizeof(*v), pos);
}

void
pb_read_str(struct pbuf* pbuf, char* str, int maxlen, int* pos)
{
    uint32_t strlen;
    /* Read the string length */
    pb_read_arrl(pbuf, &strlen, sizeof(strlen), pos);
    assert(strlen < maxlen - 1);
    /* Read the string and padding */
    pb_read(pbuf, str, strlen, pos);
    str[strlen] = '\0';
    pb_alignl(pos);
}

