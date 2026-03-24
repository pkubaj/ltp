# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2025-2026 Intel - http://www.intel.com/

top_srcdir		?= ../../../..

include $(top_srcdir)/include/mk/env_pre.mk

SUBMODULE_DIR       := $(top_srcdir)/testcases/kernel/mce
MCEINJECT_DATA_DIR  := $(prefix)/testcases/data/mce-inject/

all: mcelog-make mce-inject-make

install: mcelog-install mce-inject-install

mcelog-make:
	git submodule update --init --remote $(SUBMODULE_DIR)/mcelog
	cd $(SUBMODULE_DIR)/mcelog && make

mce-inject-make:
	git submodule update --init --remote $(SUBMODULE_DIR)/mce-inject
	cd $(SUBMODULE_DIR)/mce-inject && make

mcelog-install:
	cd $(SUBMODULE_DIR)/mcelog && make install

mce-inject-install:
	cd $(SUBMODULE_DIR)/mce-inject && make install
	mkdir -p $(MCEINJECT_DATA_DIR)
	cp -r $(SUBMODULE_DIR)/mce-inject/test $(MCEINJECT_DATA_DIR)

.PHONY: check

include $(top_srcdir)/include/mk/testcases.mk
include $(top_srcdir)/include/mk/generic_leaf_target.mk
