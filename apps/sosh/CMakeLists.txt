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

project(sosh C)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u __vsyscall_ptr")

add_executable(sosh EXCLUDE_FROM_ALL src/benchmark.c src/sosh.c)
target_include_directories(sosh PRIVATE include)
target_link_libraries(sosh sel4runtime muslc sel4 sosapi utils)

# warn about everything
add_compile_options(-Wall -Werror -W -Wextra)

add_app(sosh)
