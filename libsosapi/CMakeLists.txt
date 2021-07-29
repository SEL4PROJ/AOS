#
# Copyright 2019, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(DATA61_GPL)
#
cmake_minimum_required(VERSION 3.7.2)

project(libsosapi C)

add_library(
    sosapi
    EXCLUDE_FROM_ALL
    src/sos.c
    src/sys_exit.c
    src/sys_morecore.c
    src/sys_stdio.c
    src/sys_thread.c
    src/sys_time.c
    src/vsyscall.c
)
target_link_options(sosapi BEFORE INTERFACE "-Wl,-usosapi_init_syscall_table")
target_include_directories(sosapi PUBLIC include)
target_link_libraries(sosapi sel4runtime muslc sel4 utils aos)

# warn about everything
add_compile_options(-Wall -Werror -W -Wextra)
