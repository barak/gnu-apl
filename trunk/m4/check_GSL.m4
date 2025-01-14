
# check if libgsl is installed and usable

# we believe that function gsl_linalg_QL_decomp() was added in libgsl 2.7.
# So we simply check the version with the presence of gsl_linalg_QL_decomp().

apl_GSL=yes
AC_CHECK_LIB([gsl], [gsl_linalg_QL_decomp], , apl_GSL=no)
AC_CHECK_HEADER([gsl/gsl_version.h], , apl_GSL=no)
AM_CONDITIONAL(apl_GSL, apl_YES($apl_GSL))

