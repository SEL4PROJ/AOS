/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* transport.h */

#ifndef __RPC_H
#define __RPC_H

#include <lwip/udp.h>
#include <nfs/nfs.h>

enum port_type {
    PORT_ANY,
    PORT_ROOT
};

/* RPC Reply header fields */
enum rpc_reply_err {
    RPCERR_OK           =  0,
    RPCERR_BAD_MSG      = -1,
    RPCERR_NOT_ACCEPTED = -2,
    RPCERR_FAILURE      = -3,
    RPCERR_NOT_OK       = -4,
    RPCERR_NOT_FOUND    = -5,
    RPCERR_NEXT_AVAIL   = -6
};

enum msg_type {
    MSG_CALL  = 0,
    MSG_REPLY = 1
};

enum reply_stat {
    MSG_ACCEPTED = 0,
    MSG_DENIED   = 1
};

struct rpc_reply_hdr {
    uint32_t xid;
    uint32_t msg_type;
    uint32_t reply_stat;
};


/*******************************
 *** Transport layer interface 
 *******************************/

/* rpc callback functions called when a packet is received */
typedef void (*rpc_cb_fn)(void* cb, uintptr_t token, struct pbuf* pbuf);

/**
 *  initialise that transport layer
 * @param[in] server  The IP address of a time server
 * @return          0 on successful initialisation
 */
int init_rpc(const struct ip_addr *server);

/**
 * Create a new udp pcb for use with the rpc_send and rpc_call functions 
 * @param[in] server      The IP address of the server to connect to
 * @param[in] remote_port The remote port to connect to
 * @param[in] local_port  The range of port addresses to use.
 * @return On success; return a reference to the newly created udp pcb
 *         Otherwise; NULL.
 */
struct udp_pcb* rpc_new_udp(const struct ip_addr* server, int remote_port, 
                            enum port_type local_port);

/**
 * Allocates a pbuf and writes the rpc header 
 * @param[in] prog The program number
 * @param[in] vers The version number
 * @param[in] proc The proceedure number
 * @param[out] pos The position is assumed to initially be zero. The
 *                 value at this address will contain the next position to
 *                 write to when the call completes.
 * @return On success; A reference to the newly allocated pbuf.
 *         Otherwise; NULL.
 */
struct pbuf * rpcpbuf_init(int prog, int vers, int proc, int* pos);

/**
 * Read and check the rpc header from the pbuf
 * @param[in] pbuf A reference to the pbuf to probe
 * @param[out] hdr A reference to a rpc header structure to fill.
 * @param[out] pos The position is assumed to initially be zero. The
 *                 value at this address will contain the next position to
 *                 read from when the call completes.
 * Return rpc error code
 */
enum rpc_reply_err rpc_read_hdr(struct pbuf* pbuf, 
                                struct rpc_reply_hdr* hdr, int* pos);


/**
 * Send an RPC packet, add a callback for this packet to the queue and
 * retransmitt as necessary. 
 * Free the pbuf only once the response is handled.
 * @param[in] pbuf     The pbuf to send
 * @param[in] len      The length of the payload
 * @param[in] pcb      The connection used for the transmission
 * @param[in] func     A callback to register when a reponse has been received.
 * @param[in] callback First argument to 'func'
 * @param[in] token    Second argument to 'func'
 * @return             RPC_OK if the request was successfully sent. Otherwise,
 *                     and appropriate error is returned.
 */
enum rpc_stat rpc_send(struct pbuf *pbuf, int len, struct udp_pcb *pcb, 
                       rpc_cb_fn func, void *callback, uintptr_t token);

/**
 * Send am RPC packet and wait for a response before returning.
 * Frees the pbuf after use
 * @param[in] pbuf     The pbuf to send
 * @param[in] len      The length of the payload
 * @param[in] pcb      The pcb to use for transmission
 * @param[in] func     A callback function for the response
 * @param[in] callback The first argument of the callback function
 * @param[in] token    The second argument of the callback funtion
 * @return             RPC_OK if the request was successfully sent. Otherwise,
 *                     and appropriate error is returned.
 */
enum rpc_stat rpc_call(struct pbuf *pbuf, int len, struct udp_pcb *pcb, 
                       rpc_cb_fn func, void *callback, uintptr_t token);


/**
 * Retransmit packets as necessary
 * @param ms  The number of elapsed milliseconds since the last call
 */
void rpc_timeout(int ms);

#endif /* __RPC_H */
