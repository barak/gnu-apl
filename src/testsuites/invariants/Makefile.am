
# APL2 LRM Chapter 5 invariant testcases
#
# Each .tc file tests the invariants for one primitive function or operator,
# using ⎕RVAL to generate fresh random arguments on every run.
#
# test suite invariants (make test4). Run with:
#   cd src; ./apl --id 1010 --TL testsuites/invariants

# put them into apl-x.x.tar.gz, but don't install them. zzzz_selection lists
# the active testcases (see ../README).
#
dist_noinst_DATA =              \
        zzzz_selection          \
        Add.tc                  \
        BracketIndex.tc         \
        Catenate.tc             \
        CatenateAxis.tc         \
        Ceiling.tc              \
        CircleFunctions.tc      \
        Compress.tc             \
        CompressAxis.tc         \
        Deal.tc                 \
        Decode.tc               \
        Depth.tc                \
        Disclose.tc             \
        DiscloseAxis.tc         \
        Divide.tc               \
        Drop.tc                 \
        DropAxis.tc             \
        EachDyadic.tc           \
        EachMonadic.tc          \
        FastPath.tc             \
        Enclose.tc              \
        EncloseAxis.tc          \
        Encode.tc               \
        Enlist.tc               \
        Expand.tc               \
        ExpandAxis.tc           \
        Find.tc                 \
        First.tc                \
        Format.tc               \
        GradeDown.tc            \
        GradeDownCollating.tc   \
        GradeUp.tc              \
        GradeUpCollating.tc     \
        Index.tc                \
        IndexOf.tc              \
        InnerProduct.tc         \
        Interval.tc             \
        Laminate.tc             \
        Logarithm.tc            \
        Magnitude.tc            \
        Match.tc                \
        MatrixDivide.tc         \
        MatrixInverse.tc        \
        Member.tc               \
        Multiply.tc             \
        Negative.tc             \
        OuterProduct.tc         \
        Partition.tc            \
        PartitionAxis.tc        \
        Pick.tc                 \
        Power.tc                \
        Ravel.tc                \
        RavelAxis.tc            \
        Reciprocal.tc           \
        Reduce.tc               \
        ReduceNWise.tc          \
        ReduceAxis.tc           \
        Replicate.tc            \
        ReplicateAxis.tc        \
        Reshape.tc              \
        Reverse.tc              \
        ReverseAxis.tc          \
        Rotate.tc               \
        RotateAxis.tc           \
        Scan.tc                 \
        ScanAxis.tc             \
        Shape.tc                \
        Subtract.tc             \
        Take.tc                 \
        TakeAxis.tc             \
        Transpose.tc            \
        TransposeReversed.tc    \
        Without.tc

# remove the log files of the last run (*.tc.log, summary.log)
.PHONY: clear-log
clear-log:
	rm -f *.log
