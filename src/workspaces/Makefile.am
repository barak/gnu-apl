
# workspaces and files that may be of interest

# put them into apl-x.x.tar.gz, but don't install them.
#
dist_noinst_DATA =		\
	FORMAT_FUZZ.apl		\
	Performance.apl		\
	Performance.gnuplot	\
	qq111.atf

# ASSERT.apl (library 0, needed by testcases that do ")COPY ASSERT.apl" or
# ")COPY_ONCE 0 ASSERT.apl", e.g. Rank.tc and Polynomials.tc) is generated
# here from the single SVN-tracked master copy in wslib5/ASSERT.apl instead
# of keeping a second, driftable copy under version control.
#
ASSERT.apl: $(top_srcdir)/wslib5/ASSERT.apl
	cp $< $@

all-local: ASSERT.apl

CLEANFILES = ASSERT.apl

