
# this file (either Makefile.dist or Makefile) is the inititial version of
# Makefile in the top-level directory of GNU APL. It provides a short-cut
# to those make targets that do not need ./configure, and it runs ./configure
# for the other make targets (i.e. those that need it).
# to be made without running ./configure first.
#
# If this file is Makefile, then ./configure will override it with a "proper"
# Makefile that is derived from Makefile.in.
#
# Otherwise (i.e. this file is Makefile.dist) this file is not touched so that
# the initial Makefile can be re-created (e.g.  # after make distclean) with:
#
#     cp Makefile.dist Makefile
#

# 1. targets defined in Makefile.incl can be built before ./configure
#
.PHONY: help develop develop_lib parallel parallel1
help develop develop_lib parallel parallel1:
	make -f Makefile.incl $@

# 2. all other targets shall run ./configure first
#
%:
	rm -f Makefile
	./configure
	make $@

