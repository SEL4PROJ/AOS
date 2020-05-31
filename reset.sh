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

PATH="${0%/*}:$PATH"

echo odroid upload-boot "${PWD}/images/sos-image-arm-odroidc2"
odroid upload-boot "${PWD}/images/sos-image-arm-odroidc2"

echo odroid upload "${PWD}/apps/*"
odroid upload "${PWD}/apps/"*

echo odroid reset
odroid reset
