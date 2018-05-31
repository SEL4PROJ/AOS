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

project(sosh C)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u __vsyscall_ptr")

file(GLOB deps src/*.c)
add_executable(sosh EXCLUDE_FROM_ALL ${deps})
target_include_directories(sosh PRIVATE include)
target_link_libraries(sosh muslc sel4 sosapi utils)

# warn about everything
add_compile_options(-Wall -Werror -W -Wextra)

set_property(GLOBAL APPEND PROPERTY apps_property "$<TARGET_FILE:sosh>")
