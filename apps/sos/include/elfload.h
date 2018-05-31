/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#pragma once

#include <sel4/sel4.h>
#include <cspace/cspace.h>
#include <elf/elf.h>
#include <elf.h>

int elf_load(cspace_t *cspace, seL4_CPtr loader_vspace, seL4_CPtr loadee_vspace, char *elf_file);
