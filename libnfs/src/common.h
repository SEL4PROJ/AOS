/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef __COMMON_H
#define __COMMON_H


/************************************************************
 *  Imports
 ***********************************************************/
extern void sos_usleep(int usecs);
#define _usleep(us) sos_usleep(us)

#endif /* __COMMON_H */
