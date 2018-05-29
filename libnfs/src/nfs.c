/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "nfs.h"
#include "rpc.h"
#include "mountd.h"
#include "portmapper.h"
#include "pbuf_helpers.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


//#define DEBUG_NFS 1
#ifdef DEBUG_NFS
#define debug(x...) printf(x)
#else
#define debug(x...)
#endif

/* The amount of data to receive needs to be limited such that the
 * number of fragments does not exceed IP_REASS_MAX_PBUFS.
 * This is a global limit and may affect performance in strange
 * ways due to retransmissions caused by failed reassembly. Hence
 * we limit to prevent any IP fragmentation; fragmentation is to be
 * done by the application layer.
 */
#define NFS_READ_MAX  1024

#define NFS_NUMBER    100003
#define NFS_VERSION   2

//void        NFSPROC_NULL(void)
#define       NFSPROC_NULL                  0
//attrstat    NFSPROC_GETATTR(fhandle)
#define       NFSPROC_GETATTR               1
//attrstat    NFSPROC_SETATTR(sattrargs)
#define       NFSPROC_SETATTR               2
//void        NFSPROC_ROOT(void)
#define       NFSPROC_ROOT                  3
//diropres    NFSPROC_LOOKUP(diropargs)
#define       NFSPROC_LOOKUP                4
//readlinkres NFSPROC_READLINK(fhandle)
#define       NFSPROC_READLINK              5
//readres     NFSPROC_READ(readargs)
#define       NFSPROC_READ                  6
//void        NFSPROC_WRITECACHE(void)
#define       NFSPROC_WRITECACHE            7
//attrstat    NFSPROC_WRITE(writeargs)
#define       NFSPROC_WRITE                 8
//diropres    NFSPROC_CREATE(createargs)
#define       NFSPROC_CREATE                9
//stat        NFSPROC_REMOVE(diropargs)
#define       NFSPROC_REMOVE                10
//stat        NFSPROC_RENAME(renameargs)
#define       NFSPROC_RENAME                11
//stat        NFSPROC_LINK(linkargs)
#define       NFSPROC_LINK                  12
//stat        NFSPROC_SYMLINK(symlinkargs)
#define       NFSPROC_SYMLINK               13
//diropres    NFSPROC_MKDIR(createargs)
#define       NFSPROC_MKDIR                 14
//stat        NFSPROC_RMDIR(diropargs)
#define       NFSPROC_RMDIR                 15
//readdirres  NFSPROC_READDIR(readdirargs)
#define       NFSPROC_READDIR               16
//statfsres   NFSPROC_STATFS(fhandle)
#define       NFSPROC_STATFS                17

/* 
 * This is the maximum amount (in bytes) of requested data that the server 
 * will return. The data returned includes cookies, file ids, file name length,
 * the file name itself and a signal for the end of the list.
 * It must be small enough to fit in a response packet but large enough to 
 * retrieve at least one entry to ensure progress.
 *
 * READDIR_BUF_SIZE > 4 longs + max name length = 4*4 + 256 = 272
 */
#define READDIR_BUF_SIZE   1024

static struct udp_pcb *_nfs_pcb = NULL;

void 
nfs_timeout(void)
{
    rpc_timeout(100);
}

enum rpc_stat
nfs_mount(const char * dir, fhandle_t *pfh)
{
    assert(_nfs_pcb);
    return mountd_mount(&_nfs_pcb->remote_ip, dir, pfh);
}

enum rpc_stat
nfs_print_exports(void)
{
    assert(_nfs_pcb);
    return mountd_print_exports(&_nfs_pcb->remote_ip);
}

/******************************************
 *** NFS Initialisation 
 ******************************************/

/* this should be called once at beginning to setup everything */
enum rpc_stat
nfs_init(const struct ip_addr *server)
{
    int port;
    /* Initialise our RPC transport layer */
    if(init_rpc(server)){
        printf("Error receiving time using UDP time protocol\n");
        return RPCERR_NOSUP;
    }

    /* make and RPC to get nfs info */
    port = portmapper_getport(server, NFS_NUMBER, NFS_VERSION);
    switch(port){
    case -1:
        printf( "Communication error when acquiring NFS port from portmapper\n" );
        return RPCERR_COMM;
    case -0:
    case -2:
        printf( "Error when acquiring NFS port number from portmapper: Not supported\n" );
        return RPCERR_NOSUP;
    default:
        debug( "NFS port number is %d\n", port);
        _nfs_pcb = rpc_new_udp(server, port, PORT_ROOT);
        assert(_nfs_pcb);
        return RPC_OK;
    }
}

/******************************************
 *** Async functions
 ******************************************/

static void
_nfs_getattr_cb(void * callback, uintptr_t token, struct pbuf *pbuf)
{
    uint32_t status = NFSERR_COMM;
    fattr_t pattrs;
    struct rpc_reply_hdr hdr; 
    int pos;
    nfs_getattr_cb_t cb = callback;

    assert(callback != NULL);

    if (rpc_read_hdr(pbuf, &hdr, &pos) == RPCERR_OK){
        /* get the status out */
        pb_readl(pbuf, &status, &pos);
        if (status == NFS_OK) {
            /* it worked, so take out the return stuff! */
            pb_read_arrl(pbuf, (uint32_t*)&pattrs, sizeof(pattrs), &pos);
        }
    }

    cb(token, status, &pattrs);

    return;
}

enum rpc_stat
nfs_getattr(const fhandle_t *fh,
            nfs_getattr_cb_t func, uintptr_t token)
{
    struct pbuf *pbuf;
    int pos;

    /* now the user data struct is setup, do some call stuff! */
    pbuf = rpcpbuf_init(NFS_NUMBER, NFS_VERSION, NFSPROC_GETATTR, &pos);
    if(pbuf == NULL){
        return RPCERR_NOBUF;
    }

    /* put in the fhandle */
    pb_write(pbuf, fh, sizeof(*fh), &pos);

    /* send it! */
    return rpc_send(pbuf, pos, _nfs_pcb, &_nfs_getattr_cb, func, token);
}

static void
_nfs_lookup_cb(void * callback, uintptr_t token, struct pbuf *pbuf)
{
    uint32_t status = NFSERR_COMM;
    fhandle_t new_fh;
    fattr_t pattrs;
    struct rpc_reply_hdr hdr; 
    int pos;
    nfs_lookup_cb_t cb = callback;

    assert(callback != NULL);

    if (rpc_read_hdr(pbuf, &hdr, &pos) == RPCERR_OK){
        /* get the status out */
        pb_readl(pbuf, &status, &pos);
        if (status == NFS_OK) {
            /* it worked, so take out the return stuff! */
            pb_read(pbuf, &new_fh, sizeof(new_fh), &pos);
            pb_read_arrl(pbuf, (uint32_t*)&pattrs, sizeof(pattrs), &pos);
        }
    }

    cb(token, status, &new_fh, &pattrs);
}

/* request a file handle */
enum rpc_stat
nfs_lookup(const fhandle_t *cwd, const char *name,
           nfs_lookup_cb_t func, uintptr_t token)
{
    struct pbuf *pbuf;
    int pos;

    /* now the user data struct is setup, do some call stuff! */
    pbuf = rpcpbuf_init(NFS_NUMBER, NFS_VERSION, NFSPROC_LOOKUP, &pos);
    if(pbuf == NULL){
        return RPCERR_NOBUF;
    }

    /* put in the fhandle */
    pb_write(pbuf, cwd, sizeof(*cwd), &pos);
    /* put in the name */
    pb_write_str(pbuf, name, strlen(name), &pos);
    /* send it! */
    return rpc_send(pbuf, pos, _nfs_pcb, &_nfs_lookup_cb, func, token);
}

static void
_nfs_read_cb(void * callback, uintptr_t token, struct pbuf *pbuf)
{
    uint32_t status = NFSERR_COMM;
    fattr_t pattrs;
    char *data = NULL;
    uint32_t size = 0;
    struct rpc_reply_hdr hdr; 
    int pos;
    nfs_read_cb_t cb = callback;


    assert(callback != NULL);

    if (rpc_read_hdr(pbuf, &hdr, &pos) == RPCERR_OK){
        /* get the status out */
        pb_readl(pbuf, &status, &pos);

        if (status == NFS_OK) {
            /* it worked, so take out the return stuff! */
            pb_read_arrl(pbuf, (uint32_t*)&pattrs, sizeof(pattrs), &pos);
            pb_readl(pbuf, &size, &pos);
            /* malloc for data since pbuf may be part of a chain */
            data = malloc(size);
            assert(data != NULL);
            pb_read(pbuf, data, size, &pos);
        }
    }

    cb(token, status, &pattrs, size, data);

    if(data){
        free(data);
    }
}

enum rpc_stat
nfs_read(const fhandle_t *fh, int offset, int count, 
         nfs_read_cb_t func, uintptr_t token)
{
    struct pbuf *pbuf;
    int pos;

    /* now the user data struct is setup, do some call stuff! */
    pbuf = rpcpbuf_init(NFS_NUMBER, NFS_VERSION, NFSPROC_READ, &pos);
    if(pbuf == NULL){
        return RPCERR_NOBUF;
    }

    /* Limit the number of bytes to receive; lwip has a limit
     * on fragmentation that would result in an infinite RX loop. */
    if (count > NFS_READ_MAX){
        count = NFS_READ_MAX;
    }

    /* Fill in the call data */
    pb_write(pbuf, fh, sizeof(*fh), &pos);
    pb_writel(pbuf, offset, &pos);
    pb_writel(pbuf, count, &pos);
    /* total count unused as per RFC */
    pb_writel(pbuf, 0, &pos);

    return rpc_send(pbuf, pos, _nfs_pcb, &_nfs_read_cb, func, token);
}

struct write_token_wrapper {
    uintptr_t token;
    int count;
};

static void
_nfs_write_cb(void * callback, uintptr_t token, struct pbuf *pbuf)
{
    struct write_token_wrapper *t = (struct write_token_wrapper*)token;
    struct rpc_reply_hdr hdr;
    uint32_t status = NFSERR_COMM;
    fattr_t pattrs;
    int pos;
    nfs_write_cb_t cb = callback;

    assert(callback != NULL);

    if (rpc_read_hdr(pbuf, &hdr, &pos) == RPCERR_OK){
        /* get the status out */
        pb_readl(pbuf, &status, &pos);
        if (status == NFS_OK) {
            /* it worked, so take out the return stuff! */
            pb_read_arrl(pbuf, (uint32_t*)&pattrs, sizeof(pattrs), &pos);
        }
    }

    cb(t->token, status, &pattrs, t->count);

    free(t);
}

enum rpc_stat
nfs_write(const fhandle_t *fh, int offset, int count, const void *data,
          nfs_write_cb_t func, uintptr_t token)
{
    struct pbuf *pbuf;
    struct write_token_wrapper *t;
    int limit;
    int pos;
    int err;

    t = (struct write_token_wrapper*)malloc(sizeof(*t));
    if(t == NULL){
        return RPCERR_NOMEM;
    }

    /* now the user data struct is setup, do some call stuff! */
    pbuf = rpcpbuf_init(NFS_NUMBER, NFS_VERSION, NFSPROC_WRITE, &pos);
    if(pbuf == NULL){
        free(t);
        return RPCERR_NOBUF;
    }

    /* Create our call arg */
    pb_write(pbuf, fh, sizeof(*fh), &pos);
    pb_writel(pbuf, 0 /* Unused: see RFC */, &pos); 
    pb_writel(pbuf, offset, &pos);
    pb_writel(pbuf, 0 /* Unused: see RFC */, &pos);
    /* Limit the number of bytes to send to fit the packet */
    limit = pbuf->tot_len - pos - sizeof(count);
    if(count > limit){
        count = limit;
    }
    /* put the data in */
    pb_writel(pbuf, count, &pos);
    pb_write(pbuf, data, count, &pos);
    pb_alignl(&pos);

    /* Wrap the token up ready for the call back */
    t->token = token;
    t->count = count;
    err = rpc_send(pbuf, pos, _nfs_pcb, &_nfs_write_cb, func, (uintptr_t)t);
    if(err){
        free(t);
    }
    return err;
}

static void
_nfs_create_cb(void * callback, uintptr_t token, struct pbuf *pbuf)
{
    uint32_t status = NFSERR_COMM;
    fhandle_t new_fh;
    fattr_t pattrs;
    struct rpc_reply_hdr hdr; 
    int pos;
    nfs_create_cb_t cb = callback;

    assert(callback != NULL);

    if (rpc_read_hdr(pbuf, &hdr, &pos) == RPCERR_OK){
        /* get the status out */
        pb_readl(pbuf, &status, &pos);

        if (status == NFS_OK) {
            /* it worked, so take out the return stuff! */
            pb_read(pbuf, &new_fh, sizeof(new_fh), &pos);
            pb_read_arrl(pbuf, (uint32_t*)&pattrs, sizeof(pattrs), &pos);
        }
    }

    debug("NFS CREATE CALLBACK\n");
    cb(token, status, &new_fh, &pattrs);
}

enum rpc_stat
nfs_create(const fhandle_t *fh, const char *name, const sattr_t *sat,
           nfs_create_cb_t func, uintptr_t token)
{
    struct pbuf *pbuf;
    int pos;

    /* now the user data struct is setup, do some call stuff! */
    pbuf = rpcpbuf_init(NFS_NUMBER, NFS_VERSION, NFSPROC_CREATE, &pos);
    if(pbuf == NULL){
        return RPCERR_NOBUF;
    }

    /* put in the fhandle */
    pb_write(pbuf, fh, sizeof(*fh), &pos);
    /* put in the name */
    pb_write_str(pbuf, name, strlen(name), &pos);
    /* put in the attributes */
    pb_write_arrl(pbuf, (uint32_t*)sat, sizeof(*sat), &pos);

    return rpc_send(pbuf, pos, _nfs_pcb, &_nfs_create_cb, func, token);
}

void
_nfs_remove_cb(void * callback, uintptr_t token, struct pbuf *pbuf)
{
    uint32_t status = NFSERR_COMM;
    struct rpc_reply_hdr hdr; 
    int pos;
    nfs_remove_cb_t cb = callback;

    assert(callback != NULL);

    if (rpc_read_hdr(pbuf, &hdr, &pos) == RPCERR_OK){
        /* get the status out */
        pb_readl(pbuf, &status, &pos);
    }

    debug("NFS REMOVE CALLBACK\n");
    cb(token, status);
}

enum rpc_stat
nfs_remove(const fhandle_t *fh, const char *name, 
           nfs_remove_cb_t func, uintptr_t token)
{
    struct pbuf *pbuf;
    int pos;

    /* now the user data struct is setup, do some call stuff! */
    pbuf = rpcpbuf_init(NFS_NUMBER, NFS_VERSION, NFSPROC_REMOVE, &pos);
    if(pbuf == NULL){
        return RPCERR_NOBUF;
    }

    /* put in the fhandle */
    pb_write(pbuf, fh, sizeof(*fh), &pos);
    /* put in the name */
    pb_write_str(pbuf, name, strlen(name), &pos);

    return rpc_send(pbuf, pos, _nfs_pcb, _nfs_remove_cb, func, token);
}


static void
_nfs_readdir_cb(void * callback, uintptr_t token, struct pbuf *pbuf)
{
    nfs_readdir_cb_t cb;
    int num_entries = 0;
    char **entries = NULL;
    uint32_t next_cookie = 0;
    uint32_t status = NFSERR_COMM;
    struct rpc_reply_hdr hdr; 
    int pos;

    debug("NFS READDIR CALLBACK\n");
    cb = callback;
    assert(callback != NULL);

    if (rpc_read_hdr(pbuf, &hdr, &pos) == RPCERR_OK){
        /* get the status out */
        pb_readl(pbuf, &status, &pos);
        if (status == NFS_OK) {
            struct filelist {
                char *file;
                struct filelist *next;
            };
            struct filelist *head = NULL;
            struct filelist **tail = &head;
            uint32_t more;
            int i;

            debug("Getting entries\n");
            /* First read all of our names into a chain */
            while(pb_readl(pbuf, &more, &pos), more) {
                uint32_t size, fileid;
                char* name = NULL;
                /* File ID (ignored) */
                pb_readl(pbuf, &fileid, &pos);
                /* Read in the file name length and file name */
                pb_readl(pbuf, &size, &pos);
                name = (char*)malloc(size + 1);
                pb_read(pbuf, name, size, &pos);
                name[size] = '\0';
                pb_alignl(&pos);
                /* Read the cookie: The last value read will be used for the
                 * next call call to nfs_readdir for more file names */
                pb_readl(pbuf, &next_cookie, &pos);
                /* update records */
                *tail = (struct filelist*)malloc(sizeof(struct filelist));
                (*tail)->file = name;
                (*tail)->next = NULL;
                tail = &(*tail)->next;
                num_entries++;
            }
            /* Now flatten the chain */
            entries = (char**)malloc(sizeof(char*)*num_entries);
            assert(entries);
            for(i = 0; i < num_entries; i++){
                struct filelist *old_head;
                assert(head);
                old_head = head;
                head = head->next;
                entries[i] = old_head->file;
                free(old_head);
            }
        }
    }

    cb(token, status, num_entries, entries, next_cookie);

    /* Clean up */
    if (entries){
        int i;
        for(i = 0; i < num_entries; i++){
            free(entries[i]);
        }
        free(entries);
    }
}

/* send a request for a directory item */
enum rpc_stat
nfs_readdir(const fhandle_t *pfh, nfscookie_t cookie,
        nfs_readdir_cb_t func, uintptr_t token)
{
    struct pbuf *pbuf;
    int pos;
    /* Initialise the request packet */
    pbuf = rpcpbuf_init(NFS_NUMBER, NFS_VERSION, NFSPROC_READDIR, &pos);
    if(pbuf == NULL){
        return RPCERR_NOBUF;
    }
    /* Create the call */
    pb_write(pbuf, pfh, sizeof(*pfh), &pos);
    pb_writel(pbuf, cookie, &pos);
    pb_writel(pbuf, READDIR_BUF_SIZE, &pos); 
    /* make the call! */
    return rpc_send(pbuf, pos, _nfs_pcb, &_nfs_readdir_cb, func, token);
}


