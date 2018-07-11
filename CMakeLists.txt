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

include(${KERNEL_HELPERS_PATH})

# add apps
add_subdirectory(apps/tty_test)
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
add_subdirectory(libserial)
add_subdirectory(libsosapi)
# add any additional libs here

# set the variables for the AOS platform
set(KernelArch arm CACHE STRING "" FORCE)
set(KernelArmSel4Arch "aarch64" CACHE STRING "" FORCE)
set(KernelARMPlatform "odroidc2" CACHE STRING "" FORCE)

# export the generic timer virtual count for delay functions
set(KernelArmExportVCNTUser ON CACHE BOOL "" FORCE)

# export the PMU so the cycle counter can be configured at user level
set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE)

# enable our networking libs
set(LibPicotcp ON CACHE BOOL "" FORCE)
set(LibPicotcpBsd ON CACHE BOOL "" FORCE)
set(LibNfs ON CACHE BOOL "" FORCE)

# Elfloader settings that correspond to how Data61 sets its boards up.
ApplyData61ElfLoaderSettings()

# domains == 1 for AOS
set(KernelNumDomains 1 CACHE STRING "")

# just 1 core
set(KernelMaxNumNodes 1 CACHE STRING "")

# turn on all the nice features for debugging
# TODO for benchmarking, you should turn these OFF.
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
set(KernelVerificationBuild OFF CACHE BOOL "" FORCE)
set(KernelIRQReporting ON CACHE BOOL "" FORCE)
set(KernelPrinting ON CACHE BOOL "" FORCE)
set(KernelDebugBuild ON CACHE BOOL "" FORCE)
