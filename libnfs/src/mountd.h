/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/**
 * @file   mountd.h
 * @date   Sun Jul 7 21:03:06 2013
 *
 * @brief  Mount daemon client for the Network File System (NFS) client
 *
 * Provides simple mountd client calls to a mountd server. The caller may
 * print a list of exports hosted by the server to the screen or mount
 * a specific directory from a server.
 */

#ifndef __MOUNTD_H
#define __MOUNTD_H

#include <nfs/nfs.h>
#include <lwip/ip_addr.h>


/**
 * Mounts a directory over the network
 * @param[in]  server The IP address of the server to mount from
 * @param[in]  dir    The name of the directory to mount
 * @param[out] pfh    If the value returned is RPC_OK, "pfh" contains an
 *                    file handle to the mounted path.
 * @return            RPC_OK on success, otherwise and appropriate error
 *                    code it returned.
 */
enum rpc_stat mountd_mount(const struct ip_addr *server, const char *dir, 
                           fhandle_t *pfh);

/**
 * Prints the directories exported by a server
 * @param[in] server  The IP address of the server to query
 * @return            RPC_OK on success, otherwise and appropriate error
 *                    code it returned.
 */
enum rpc_stat mountd_print_exports(const struct ip_addr *server);



#endif /* __MOUNTD_H */
