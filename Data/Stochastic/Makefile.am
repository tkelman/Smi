# Copyright (C) 2006 International Business Machines and others.
# All Rights Reserved.
# This file is distributed under the Eclipse Public License.

## $Id$

# Author:  Andreas Waechter           IBM    2006-04-13

AUTOMAKE_OPTIONS = foreign

########################################################################
#                List files that should be distributed                 #
########################################################################

EXTRA_DIST = $(EXAMPLE_FILES)

DISTCLEANFILES = $(EXAMPLE_CLEAN_FILES)

datacoindir = $(datadir)/coin/Data/Stochastic
datacoin_DATA = $(EXAMPLE_FILES)

pkgconfiglibdir = $(libdir)/pkgconfig
pkgconfiglib_DATA = coindatastochastic.pc

test:
	@echo "No test available."
	
.PHONY: test

include BuildTools/Makemain.inc
