
# check if libgsl is installed and usable

# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_GSL()
{ {
apl_GSL=no

m4_include([m4/ax_path_lib_gsl.m4])   # define AX_PATH_LIB_GSL()
AX_PATH_LIB_GSL([])                   # set ac_cv_lib_gsl_32_gsl_compile_32
apl_GSL=$ac_cv_lib_gsl_gsl_blas_dgemm

# export apl_GSL to Makefile.am
AM_CONDITIONAL(apl_GSL, apl_YES($apl_GSL))
} }
dash_test_GSL   # set apl_GSL to yes or no.
