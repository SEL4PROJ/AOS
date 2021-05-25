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

# set the build platform
set(PLATFORM odroidc2 CACHE STRING "" FORCE)

# build all libs as static
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(project_dir "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(resolved_path ${CMAKE_CURRENT_LIST_FILE} REALPATH)
# repo_dir is distinct from project_dir as this file is symlinked.
# project_dir corresponds to the top level project directory, and
# repo_dir is the absolute path after following the symlink.
get_filename_component(repo_dir ${resolved_path} DIRECTORY)

include(${project_dir}/tools/seL4/cmake-tool/helpers/application_settings.cmake)

correct_platform_strings()

include(${project_dir}/kernel/configs/seL4Config.cmake)

function(add_app app)
    set(destination "${CMAKE_BINARY_DIR}/apps/${app}")
    set_property(GLOBAL APPEND PROPERTY apps_property "$<TARGET_FILE:${app}>")
    add_custom_command(
        TARGET ${app} POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${app}> ${destination} BYPRODUCTS ${destination}
    )
endfunction()

# set the variables for the AOS platform

# export the generic timer virtual count for delay functions
set(KernelArmExportVCNTUser ON CACHE BOOL "" FORCE)

# export the PMU so the cycle counter can be configured at user level
set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE)

# domains == 1 for AOS
set(KernelNumDomains 1 CACHE STRING "")

# just 1 core
set(KernelMaxNumNodes 1 CACHE STRING "")

# Enable MCS
set(KernelIsMCS ON CACHE BOOL "" FORCE)

# Elfloader settings that correspond to how Data61 sets its boards up.
ApplyData61ElfLoaderSettings(${KernelPlatform} ${KernelSel4Arch})

# turn on all the nice features for debugging
# TODO for benchmarking, you should turn these OFF.
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
set(KernelVerificationBuild OFF CACHE BOOL "" FORCE)
set(KernelIRQReporting ON CACHE BOOL "" FORCE)
set(KernelPrinting ON CACHE BOOL "" FORCE)
set(KernelDebugBuild ON CACHE BOOL "" FORCE)

# enable our networking libs
set(LibPicotcp ON CACHE BOOL "" FORCE)
set(LibPicotcpBsd ON CACHE BOOL "" FORCE)
set(LibNfs ON CACHE BOOL "" FORCE)
