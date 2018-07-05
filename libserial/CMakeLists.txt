#
# Copyright 2018, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

cmake_minimum_required(VERSION 3.7.2)

project(libserial C)

add_library(serial EXCLUDE_FROM_ALL src/serial.c)
target_include_directories(serial PUBLIC include)
target_link_libraries(serial Configuration muslc sel4 picotcp utils)
