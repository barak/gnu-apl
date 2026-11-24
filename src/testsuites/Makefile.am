
# test suites: one subdirectory per test suite, containing its testcases
# (.tc files) and the file zzzz_selection that lists the active ones. Run with
# apl --TL testsuites/SUITE or apl -T @testsuites/SUITE (see README). Not
# installed.
#
# A new test suite must be listed in DIST_SUBDIRS (and in AC_CONFIG_FILES of
# configure.ac), a new testcase in the Makefile.am of its suite (so that it
# goes into the tarball). Every test suite Makefile.am needs a target
# clear-log (used by make clear-logs below).
#
EXTRA_DIST = README

DIST_SUBDIRS = main extra regression invariants

# remove the log files of all test suites: make clear-log in every
# subdirectory that has a Makefile, so that new test suites are included
# automatically.
.PHONY: clear-logs
clear-logs:
	@for d in */; do \
	  if test -f "$$d/Makefile"; then $(MAKE) -C "$$d" clear-log || exit 1; fi; \
	done
