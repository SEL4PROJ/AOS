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

# add apps
add_subdirectory(apps/console_test)
add_subdirectory(apps/sosh)
# add any additional apps here

# add sos itself, this is your OS
# and root task
add_subdirectory(sos)

# add libraries
add_subdirectory(libaos)
add_subdirectory(libclock)
add_subdirectory(libethernet)
add_subdirectory(libsel4cspace)
add_subdirectory(libnetworkconsole)
add_subdirectory(libsosapi)
# add any additional libs here

# enable our networking libs
set(LibPicotcp ON CACHE BOOL "" FORCE)
set(LibPicotcpBsd ON CACHE BOOL "" FORCE)
set(LibNfs ON CACHE BOOL "" FORCE)
