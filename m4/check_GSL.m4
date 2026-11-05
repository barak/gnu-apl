
# check if libgsl is installed and usable

# we believe that function gsl_linalg_QL_decomp() was added in libgsl 2.7.
# So we simply check the version with the presence of gsl_linalg_QL_decomp().

AC_ARG_WITH([gsl],
    AS_HELP_STRING([--with-gsl],
    [enable GSL-based extras (⌹ rank/condition estimate, ⎕MX; needs libgsl)]))
    #
    # ./configure --with-gsl="no"   →    $with_gsl: "no"
    # ./configure --without-gsl     →    $with_gsl: "no"
    # ./configure                   →    $with_gsl: "yes"
    # ./configure --with-gsl        →    $with_gsl: "yes"
    # ./configure --with-gsl=yes    →    $with_gsl: "yes"
    #
apl_GSL=no
if apl_NO($with_gsl); then       # user has explicitly disabled GSL
   AC_MSG_CHECKING([for GSL])
   AC_MSG_RESULT([no - (explicitly disabled by user)])
else
   apl_GSL=yes
   AC_CHECK_LIB([gslcblas], [cblas_cgemv],     , apl_GSL=no)
   AC_CHECK_HEADER([gsl/gsl_blas.h],           , apl_GSL=no)
   AC_CHECK_LIB([gsl], [gsl_linalg_QL_decomp], , apl_GSL=no)
   AC_CHECK_HEADER([gsl/gsl_version.h],        , apl_GSL=no)
fi
AM_CONDITIONAL(apl_GSL, apl_YES($apl_GSL))

