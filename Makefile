#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# Top level makefile.
# Type "make <thing to build>" in this directory and it will be built.

TFTPROOT = /var/tftpboot/$(USER)

DEFINES += SOS_NFS_DIR='"$(TFTPROOT)"'

ifeq ($(OS), Darwin)
	SERIAL_PORT = $(firstword $(wildcard /dev/cu.usbserial-*))
else
	SERIAL_PORT = $(firstword $(wildcard /dev/ttyUSB*))
endif


-include .config

lib-dirs:=libs

include tools/common/project.mk


all: app-images
	mkdir -p $(TFTPROOT)
	cp -v $(IMAGE_ROOT)/sos-image-arm-imx6 $(TFTPROOT)/bootimg.elf
	$(MAKE) reset

.PHONY: reset
ifeq ($(SERIAL_PORT),)
reset:
	@echo "Warning: USB serial port not found." || true
else
reset:
	@echo "Resetting sabre @ $(SERIAL_PORT)"
	@echo "reset" >> $(SERIAL_PORT)
endif



.PHONY: docs
docs: common
	mkdir -p docs
	@for doc in $(libdocs-y);                  \
	do                                         \
	  set -e;                                  \
	  echo "Building docs for $$doc";          \
	  $(MAKE) --directory "libs/$$doc" docs;   \
	  cp "libs/$$doc/docs/$$doc.pdf" ./docs;   \
	done



