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

project(libnetworkconsole C)

add_library(networkconsole EXCLUDE_FROM_ALL src/networkconsole.c)
target_include_directories(networkconsole PUBLIC include)
target_link_libraries(networkconsole muslc sel4 picotcp utils sos_Config)
