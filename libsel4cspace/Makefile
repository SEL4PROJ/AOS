#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# Targets
TARGETS := libsel4cspace.a

# Source files required to build the target
CFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))

# Header files/directories this library provides
HDRFILES := $(wildcard $(SOURCE_DIR)/include/*)

include $(SEL4_COMMON)/common.mk

.PHONY: docs
docs: docs/libsel4cspace.pdf

.PHONY: docs/libsel4cspace.pdf
docs/libsel4cspace.pdf: docs/Doxyfile include/cspace/cspace.h
	doxygen docs/Doxyfile
	$(MAKE) --directory=docs/latex
	cp docs/latex/refman.pdf docs/libsel4cspace.pdf


