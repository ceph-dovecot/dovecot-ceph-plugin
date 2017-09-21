#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

if BUILD_DICT_RADOS
DICT_RADOS = dict-rados
endif

if BUILD_STORAGE_RBOX
STORAGE_RBOX = storage-rbox
endif

if BUILD_TESTS
PLUGIN_TESTS = tests
endif

SUBDIRS = \
    librmb \
	$(DICT_RADOS) \
    $(STORAGE_RBOX) \
	$(PLUGIN_TESTS)

EXTRA_DIST = googletest

