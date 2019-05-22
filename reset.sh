#!/bin/bash
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
set -e

TFTPROOT=/var/tftpboot/${USER}
SERIAL_PORT=/dev/ttyUSB0
echo "cp ${PWD}/images/sos-image-arm-odroidc2 ${TFTPROOT}"
cp ${PWD}/images/sos-image-arm-odroidc2 ${TFTPROOT}
echo "cp ${PWD}/apps/* ${TFTPROOT}"
cp ${PWD}/apps/* ${TFTPROOT}
echo "echo \"reset\" >> ${SERIAL_PORT}"
echo "reset" >> ${SERIAL_PORT}
