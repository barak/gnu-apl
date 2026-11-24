# test suite extra: slow testcases, and testcases that need a special setup
# (make test runs extra, main and regression)
#
# put them into apl-x.x.tar.gz, but don't install them. zzzz_selection lists
# the active testcases (see ../README).
#
dist_noinst_DATA = \
        zzzz_selection             \
        AP100.tc                   \
        AP210.tc                   \
        APnnn.tc                   \
        AllExpressions.tc          \
        AllPrimitives.tc           \
        FORMAT_CPLX.tc             \
        FORMAT_PARENS.tc           \
        FORMAT_STRICT.tc           \
        File_IO_1.tc               \
        MatrixProduct.tc           \
        ZZ69_Regression.tc         \
        apl2lrm.tc

# remove the log files of the last run (*.tc.log, summary.log)
.PHONY: clear-log
clear-log:
	rm -f *.log
