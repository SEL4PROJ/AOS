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

# source files in sos
file (GLOB static src/*.c crt/*.S src/sys/*.c)

# warn about everything
add_compile_options(-Wall -Werror -W -Wextra)

# list of apps to include in the cpio archive
get_property(apps GLOBAL PROPERTY apps_property)
MakeCPIO(archive.o "${apps}")

add_executable(sos EXCLUDE_FROM_ALL ${static} archive.o)
target_include_directories(sos PRIVATE "include")
target_link_libraries(sos Configuration muslc sel4 elf cpio ethdrivers serial clock sel4cspace platsupport aos utils sosapi)

# Set this image as the rootserver
DeclareRootserver(sos)
