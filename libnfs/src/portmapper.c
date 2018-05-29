/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "portmapper.h"
#include "rpc.h"
#include "pbuf_helpers.h"

#include <assert.h>

//#define DEBUG_PMAP 1
#ifdef DEBUG_PMAP
#define debug(x...) printf(x)
#else
#define debug(x...)
#endif

/* we will only implement the getmap funciton */
#define PMAP_PORT     111      /* portmapper port number */

#define PMAP_NUMBER   100000
#define PMAP_VERSION  2

#define IPPROTO_TCP 6      /* protocol number for TCP/IP */
#define IPPROTO_UDP 17     /* protocol number for UDP/IP */

#define PMAPPROC_GETPORT 3


static void
getport_cb(void *callback, uintptr_t token, struct pbuf *pbuf)
{
    struct rpc_reply_hdr reply_hdr;
    uint32_t *port = (uint32_t*)token;
    int pos;
    (void)callback;
    if(rpc_read_hdr(pbuf, &reply_hdr, &pos)){
        *port = 0;
    }else{
        pb_readl(pbuf, port, &pos);
    }
}

int 
portmapper_getport(const struct ip_addr *server, uint32_t prog, uint32_t vers)
{
    struct udp_pcb* rpc_pcb;
    struct pbuf *pbuf;
    int pos;
    uint32_t port;
    int err;

    rpc_pcb = rpc_new_udp(server, PMAP_PORT, PORT_ANY);
    assert(rpc_pcb);

    debug("Getting port\n");
    /* Initialise the request packet */
    pbuf = rpcpbuf_init(PMAP_NUMBER, PMAP_VERSION, PMAPPROC_GETPORT, &pos);
    assert(pbuf != NULL);

    /* Fill the call data */
    pb_writel(pbuf, prog, &pos);
    pb_writel(pbuf, vers, &pos);
    pb_writel(pbuf, IPPROTO_UDP, &pos);
    pb_writel(pbuf, 0, &pos);

    /* Make the call */
    err = rpc_call(pbuf, pos, rpc_pcb, &getport_cb, NULL, (uintptr_t)&port);
    udp_remove(rpc_pcb);
    if(err){
        debug("Portmapper: RPC failed\n");
        return -1;
    }else if(port == 0){
        debug("Portmapper: Error response from server\n");
        return -2;
    }else{
        debug("Portmapper: Got port %d\n", port);
        return port;
    }
}


