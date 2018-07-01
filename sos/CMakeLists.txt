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

project(sos C)

set(configure_string "")

config_string(SosNFSDir SOS_NFS_DIR
    "NFS directory"
    DEFAULT "/var/tftpboot/$ENV{USER}/"
)

config_string(SosNetMask SOS_NETMASK
    "Network mask"
    DEFAULT "255.255.255.0"
)

config_string(SosIP SOS_IP
    "IP address"
    DEFAULT "192.168.168.2"
)

config_string(SosGateway SOS_GATEWAY
    "Gateway IP address"
    DEFAULT "192.168.168.1"
)

add_config_library(sos "${configure_string}")

# warn about everything
add_compile_options(-Wall -Werror -W -Wextra)

# list of apps to include in the cpio archive
get_property(apps GLOBAL PROPERTY apps_property)
MakeCPIO(archive.o "${apps}")

# add any new c files here
add_executable(sos EXCLUDE_FROM_ALL crt/sel4_crt0.S src/bootstrap.c src/dma.c src/elf.c src/main.c
               src/mapping.c src/network.c src/ut.c src/tests.c src/sys/backtrace.c src/sys/sys_exit.c
               src/sys/sys_morecore.c src/sys/sys_stdio.c src/sys/sys_thread.c src/sys/sys_time.c
               src/drivers/uart.c archive.o)
target_include_directories(sos PRIVATE "include")
target_link_libraries(sos Configuration muslc sel4 elf cpio serial clock sel4cspace aos utils picotcp nfs)

# Set this image as the rootserver
DeclareRootserver(sos)
