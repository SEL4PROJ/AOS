/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <nfs/nfs.h>

#include "rpc.h"
#include <nfs/time.h>
#include "pbuf_helpers.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


//#define DEBUG_RPC
#ifdef DEBUG_RPC
#define debug(x...) printf( x )
#else
#define debug(x...)
#endif

/************************************************************
 *  Constants
 ***********************************************************/
#define SRPC_VERSION 2

/* AUTH_UNIX: 9.2 http://tools.ietf.org/html/rfc1057            *
 * 'stamp' is arbitrary ID which the caller machine may generat */
#define AUTH_STAMP 37
#define ROOT 0
#define NFS_MACHINE_NAME "boggo"


#define CALL_TIMEOUT_MS 10
#define CALL_RETRIES     5

#define ROOT_PORT_MIN 45
#define ROOT_PORT_MAX 1024

#define UDP_PAYLOAD 1400

#define RETRANSMIT_DELAY_MS 500


/************************************************************
 *  Structures
 ***********************************************************/

typedef uint32_t xid_t;

struct prog_mismatch {
    uint32_t low;
    uint32_t high;
};


typedef struct auth_hdr {
    uint32_t flavour;
    uint32_t size;
    /* opaque body<400>; */
} auth_t;

typedef struct rpc_call_hdr {
    uint32_t xid;
    uint32_t type;
    uint32_t rpcvers;
    uint32_t prog;
    uint32_t vers;
    uint32_t proc;
    /* opaque_auth  cred; */
    /* opaque_auth  verf; */
    /* procedure specific data */
} call_body_hdr_t;

enum accept_stat {
    SUCCESS       = 0, // RPC executed successfully
    PROG_UNAVAIL  = 1, // remote hasn't exported program
    PROG_MISMATCH = 2, // remote can't support version #
    PROC_UNAVAIL  = 3, // program can't support procedure
    GARBAGE_ARGS  = 4  // procedure can't decode params
};

enum auth_flavor {
    AUTH_NULL       = 0,
    AUTH_UNIX       = 1,
    AUTH_SHORT      = 2,
    AUTH_DES        = 3
    /* and more to be defined */
};




/************************************************************
 *  Helpers
 ***********************************************************/
#define ROUNDDOWN(v, r) ((v) - ((v) & ((r) - 1)))
#define ROUNDUP(v, r)   ROUNDDOWN(v + (r) - 1, r)

static inline struct pbuf*
pbuf_new(u16_t length)
{
    return pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
}

static inline enum rpc_stat
my_udp_send(struct udp_pcb* pcb, struct pbuf *pbuf)
{
    int err;
    struct pbuf *p;
    /* LWIP does not preserve the pbuf so make a copy first */
    p = pbuf_new(pbuf->tot_len);
    if(p == NULL){
        return RPCERR_NOBUF;
    }
    err = pbuf_copy(p, pbuf);
    if(err){
        pbuf_free(p);
        return err;
    }

    err = udp_send(pcb, p);
    pbuf_free(p);
    switch(err){
    case ERR_MEM:
        return RPCERR_NOMEM;
    case ERR_RTE:
        return RPCERR_COMM;
    case ERR_OK:
        return RPC_OK;
    }
    return err;
}

/************************************************************
 *  Transaction ID's
 ***********************************************************/
static xid_t cur_xid = 100;

static xid_t
extract_xid(struct pbuf *pbuf)
{
    xid_t xid;
    int pos = 0;
    /* extract the xid */
    pb_readl(pbuf, &xid, &pos);
    return xid;
}

static void
seed_xid(uint32_t seed)
{
    cur_xid = seed * 10000;
}

static xid_t
get_xid(void)
{
    return ++cur_xid;
}


/************************************************************
 *  Mailboxes
 ***********************************************************/

struct rpc_queue {
    struct udp_pcb *pcb;
    struct pbuf *pbuf;
    xid_t xid;
    int timeout;
    struct rpc_queue *next;
    void (*func) (void *, uintptr_t, struct pbuf *);
    void *callback;
    uintptr_t arg;
};

static struct rpc_queue *queue = NULL;

/* 
 * Poll to see if packets should be resent.
 * Packet loss can be simulated using the following command on the
 * server:
 * sudo tc qdisc add dev eth0 root netem loss 50%
 * sudo tc qdisc show dev eth0
 * sudo tc qdisc del dev eth0 root netem loss 50%
 */
void
rpc_timeout(int ms)
{
    struct rpc_queue *q_item;
    for (q_item = queue; q_item != NULL; q_item = q_item->next) {
        q_item->timeout += ms;
        if (q_item->timeout > RETRANSMIT_DELAY_MS) {
            debug("rpc_timeout: Retransmission of 0x%08x\n", q_item->xid);
            if(my_udp_send(q_item->pcb, q_item->pbuf)){
                /* Try again later */
            }else{
                q_item->timeout = 0;
            }
        }
    }
}


static void
add_to_queue(struct pbuf *pbuf, struct udp_pcb* pcb, 
         void (*func)(void *, uintptr_t, struct pbuf *),
         void *callback, uintptr_t arg)
{
    /* Need a lock here */
    struct rpc_queue *q_item;
    struct rpc_queue *tmp;
    q_item = malloc(sizeof(struct rpc_queue));
    assert(q_item != NULL);

    q_item->next = NULL;
    q_item->pbuf = pbuf;
    q_item->xid = extract_xid(pbuf);
    q_item->pcb = pcb;
    q_item->timeout = 0;
    q_item->func = func;
    q_item->arg = arg;
    q_item->callback = callback;

    if (queue == NULL) {
        /* Add at start of the linked list */
        queue = q_item;
    } else {
        /* Add to end of the linked list */
        for(tmp = queue; tmp->next != NULL; tmp = tmp->next)
            ;
        tmp->next = q_item;
    }
}

/* Remove item from the queue -- doesn't free the memory */
static struct rpc_queue *
get_from_queue(xid_t xid)
{
    struct rpc_queue *tmp, *last = NULL;

    for (tmp = queue; tmp != NULL && tmp->xid != xid; tmp = tmp->next) {
        last = tmp;
        ;
    }
    if (tmp == NULL) {
        return NULL;
    } else if (last == NULL) {
        queue = tmp->next;
    } else {
        last->next = tmp->next;
    }

    return tmp;
}

/**********************************
 *** RPC transport
 **********************************/

static void
my_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
    struct ip_addr *addr, u16_t port)
{
    xid_t xid;
    struct rpc_queue *q_item;
    (void)port;

    xid = extract_xid(p);

    q_item = get_from_queue(xid);

    debug("Recieved a reply for xid: %u (%d) %p\n", xid, p->len, q_item);
    if (q_item != NULL){
        assert(q_item->func);
        q_item->func(q_item->callback, q_item->arg, p);
        /* Clean up the queue item */
        pbuf_free(q_item->pbuf);
        free(q_item);
    }
    /* Done with the incoming packet so free it */
    pbuf_free(p);
}


enum rpc_stat
rpc_send(struct pbuf *pbuf, int len, struct udp_pcb *pcb, 
     void (*func)(void *, uintptr_t, struct pbuf *), 
     void *callback, uintptr_t token)
{
    int err;
    assert(pcb);
    pbuf_realloc(pbuf, len);
    err = my_udp_send(pcb, pbuf);
    if(!err){
        add_to_queue(pbuf, pcb, func, callback, token);
    }else{
        pbuf_free(pbuf);
    }
    return err;
}

struct rpc_call_arg {
    void (*func)(void *, uintptr_t, struct pbuf *);
    uintptr_t token;
    void* callback;
    volatile int complete;
};

static void
rpc_call_cb(void *callback, uintptr_t token, struct pbuf *pbuf)
{
    struct rpc_call_arg *call_arg = (struct rpc_call_arg*)token;
    (void)callback;
    debug("Signal function called\n");
    assert(call_arg);

    call_arg->func(call_arg->callback, call_arg->token, pbuf);

    call_arg->complete = 1;
}

enum rpc_stat
rpc_call(struct pbuf *pbuf, int len, struct udp_pcb *pcb, 
     void (*func)(void *, uintptr_t, struct pbuf *), 
     void *callback, uintptr_t token)
{
    struct rpc_call_arg call_arg;
    struct rpc_queue *q_item;
    int time_out;
    enum rpc_stat stat;
    xid_t xid;

    /* If we give up early, we must ensure that the argument remains in memory
     * just in case the packet comes in later */
    assert(pcb);
    assert(pbuf);

    /* GeneratrSend the thing with the unlock frunction as a callback */
    call_arg.func = func;
    call_arg.callback = callback;
    call_arg.token = token;
    call_arg.complete = 0;

    /* Make the call */
    xid = extract_xid(pbuf);
    stat = rpc_send(pbuf, pbuf->tot_len, pcb, &rpc_call_cb, NULL, 
                   (uintptr_t)&call_arg);
    if(stat){
        return stat;
    }

    /* Wait for the response */
    time_out = RETRANSMIT_DELAY_MS * (CALL_RETRIES + 1);
    while(time_out >= 0){
        _usleep(CALL_TIMEOUT_MS * 1000);
        if(call_arg.complete) {
            /* Success */
            return 0;
        }
        rpc_timeout(CALL_TIMEOUT_MS);
        time_out -= CALL_TIMEOUT_MS;
    }

    /* If we get here, we have failed. Data is on the stack so make sure 
     * we remove references from the queue */
    q_item = get_from_queue(xid);
    assert(q_item);
    pbuf_free(q_item->pbuf);
    free(q_item);
    return RPCERR_COMM;
}


/************************************************************
 * Initialisation 
 ***********************************************************/

int
init_rpc(const struct ip_addr *server)
{
    uint32_t time;
    time = udp_time_get(server);
    seed_xid(time);
    return time == 0;
}

struct udp_pcb* 
rpc_new_udp(const struct ip_addr *server, int remote_port, 
            enum port_type local_port)
{
    struct udp_pcb* ret;
    static int root_port = -1;
    struct ip_addr s = *server;
    ret = udp_new();
    assert(ret);
    udp_recv(ret, my_recv, NULL);
    if(local_port == PORT_ROOT){
        if(root_port >= ROOT_PORT_MAX || root_port < ROOT_PORT_MIN){
            root_port = ROOT_PORT_MIN;
            debug("Recycling ports\n");
        }
        udp_bind(ret, IP_ADDR_ANY, root_port++);
    }else{
        /* let lwip decide for itself */
    }
    udp_connect(ret, &s, remote_port);
    return ret;
}


enum rpc_reply_err
rpc_read_hdr(struct pbuf* pbuf, struct rpc_reply_hdr* hdr, int* pos)
{
    *pos = 0;
    /* read in the header */
    pb_read_arrl(pbuf, (uint32_t*)hdr, sizeof(*hdr), pos);
    if(hdr->msg_type != MSG_REPLY){
        debug( "Got a reply to something else!!\n" );
        debug( "Looking for msgtype %d\n", MSG_REPLY );
        debug( "Got msgtype %d\n", hdr->type);
        return RPCERR_BAD_MSG;

    }else if(hdr->reply_stat != MSG_ACCEPTED){
        uint32_t err;
        debug( "Message NOT accepted (%d)\n", hdr->reply_stat);
        /* extract error code */
        pb_readl(pbuf, &err, pos);
        debug( "Error code %d\n", err);
        if(err == 1) {
            /* get the auth problem */
            pb_readl(pbuf, &err, pos);
            debug( "auth_stat %d\n", err);
        }
        return RPCERR_NOT_ACCEPTED;

    }else{
        auth_t auth;
        uint32_t auth_stat;

        /* parse the auth field */
        pb_read_arrl(pbuf, (uint32_t*)&auth, sizeof(auth), pos);
        assert(auth.flavour == AUTH_NULL);
        debug("Got auth data. size is %d\n", auth.size);

        /* parse the auth status */
        pb_readl(pbuf, &auth_stat, pos);
        if(auth_stat == SUCCESS){
            return RPCERR_OK;
        }else{
            debug( "reply stat was %d\n", auth_stat);
            return RPCERR_FAILURE;
        }
    }
}

static void
rpc_write_hdr(struct pbuf* pbuf, int prog, int vers, int proc, int* pos)
{
    /** Construct header **/
    /* The rpc call header */
    struct rpc_call_hdr rpc_hdr = {
            .xid     = get_xid(),
            .type    = MSG_CALL,
            .rpcvers = SRPC_VERSION,
            .prog    = prog,
            .vers    = vers,
            .proc    = proc
        };
    /* Credentials */
    const char* host = NFS_MACHINE_NAME;
    uint32_t ids[] = {ROOT, ROOT, ROOT};
    struct auth_hdr cred = {
            .flavour = AUTH_UNIX,
            .size    = ROUNDUP(sizeof(uint32_t)/* stamp */+
                               sizeof(uint32_t)/* hostlen */ + strlen(host) +
                               sizeof(ids), 4)
        };
    struct auth_hdr verif = {
            .flavour = AUTH_NULL,
            .size    = 0
        };

    /** Dump to pbuf **/
    *pos = 0;
    /* Header */
    pb_write_arrl(pbuf, (uint32_t*)&rpc_hdr, sizeof(rpc_hdr), pos);
    /* Credentials. Variable host name makes it hard to struct */
    pb_write_arrl(pbuf, (uint32_t*)&cred, sizeof(cred), pos);
    pb_writel(pbuf, AUTH_STAMP, pos);
    pb_write_str (pbuf, host, strlen(host), pos);
    pb_write_arrl(pbuf, ids, sizeof(ids), pos);
    /* Verifier */
    pb_write_arrl(pbuf, (uint32_t*)&verif, sizeof(verif), pos);
}

struct pbuf *
rpcpbuf_init(int prognum, int vernum, int procnum, int* pos)
{
    struct pbuf* pbuf;
    pbuf = pbuf_alloc(PBUF_TRANSPORT, UDP_PAYLOAD, PBUF_RAM);
    if(pbuf) {
        rpc_write_hdr(pbuf, prognum, vernum, procnum, pos);
    }
    return pbuf;
}


