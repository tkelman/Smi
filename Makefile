# Look at and if necessary edit the following files:
# - ../Makefiles/Makefile.location
# - Makefile.Smi

###############################################################################

export CoinDir := $(shell cd ..; pwd)
export MakefileDir := $(CoinDir)/Makefiles
include ${MakefileDir}/Makefile.coin
include ${MakefileDir}/Makefile.location

###############################################################################

.DELETE_ON_ERROR:

.PHONY: default install clean library unitTest libdepend libSmi doc

default: install
libSmi:  library

libdepend:
	(cd ../Osi && ${MAKE} inst-libOsi)

install library: libdepend
	${MAKE} -f Makefile.Smi $@

doc:
	doxygen $(MakefileDir)/doxygen.conf

unitTest: install
	(cd Test && ${MAKE} unitTest)

clean: 
	@rm -rf Junk
	@rm -rf $(UNAME)*
	@rm -rf dep
	@rm -rf Test/Junk
	@rm -rf Test/$(UNAME)*
	@rm -rf Test/dep
	@rm -f unitTest
