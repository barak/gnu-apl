
# testcases that may be of interest

# the library root of the testcases (make test etc. run ./apl --libroot
# testcases/libroot). Not installed. Its files workspaces/ASSERT.apl,
# wslib5/FILE_IO.apl, and wslib5/FILE_IO1.apl are copied from the master
# copies in ../../wslib5/ by src/Makefile (TEST_LIBROOT_COPIES); the files
# below are separate copies for the testcases.
#
# accept_tc.py accepts the result of a failed testcase (see
# ../testsuites/README), Bugs39 #106
#
dist_noinst_SCRIPTS = accept_tc.py

EXTRA_DIST  = libroot/workspaces/DIJKSTRA.atf           \
              libroot/workspaces/B38_BADIO.atf          \
              libroot/workspaces/B38_BADNAMES.atf       \
              libroot/workspaces/DOMINO-APL.apl         \
              libroot/workspaces/FORMAT_FUZZ.apl        \
              libroot/workspaces/GPROF_LOOP.apl         \
              libroot/workspaces/LEGACY38.xml           \
              libroot/workspaces/LEGACY39.xml           \
              libroot/workspaces/Performance.apl        \
              libroot/workspaces/Performance.gnuplot    \
              libroot/workspaces/TEST3.atf              \
              libroot/workspaces/TF2_test.tc            \
              libroot/workspaces/qq111.atf              \
              libroot/workspaces/display/DISPLAY.atf    \
              libroot/workspaces/display/DISPLAY.fun    \
              libroot/workspaces/display/DISPLAYC.fun   \
              libroot/workspaces/display/DISPLAYG.fun   \
              libroot/wslib1/arg_parser.apl             \
              libroot/wslib1/arg_parser_script.apl      \
              libroot/wslib3/INVARIANTS.apl             \
              libroot/wslib5/ISO-13751.apl

# put them into apl-x.x.tar.gz, but don't install them.
#
dist_noinst_DATA =                              \
        0000_OBSOLETE_TESTCASES                 \
        APnnn_1011.sh                           \
        APnnn_1011.tc2                          \
        Performance.pt

tar:
	tar cvzf testcases.tgz *.pt

