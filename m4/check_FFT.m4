
# check if libfftw3 is installed and usable

# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_FFT()
{ {
apl_FFT=no   # assume error

AC_ARG_WITH([fftw],
    AS_HELP_STRING([--with-fftw],
    [enable ⎕FFT (needs libfftw3)]))
    #
    # ./configure --with-fftw="no"   →    $with_fftw: "no"
    # ./configure --without-fftw     →    $with_fftw: "no"
    # ./configure                    →    $with_fftw: "yes"
    # ./configure --with-fftw        →    $with_fftw: "yes"
    # ./configure --with-fftw=yes    →    $with_fftw: "yes"
    #
if apl_NO($with_fftw); then      # user has explicitly disabled FFTW
   AC_MSG_CHECKING([for FFTW])
   AC_MSG_RESULT([no - (explicitly disabled by user)])
   return
fi

AC_CHECK_HEADER([fftw3.h], , return)

apl_OPT_LIB([fftw3], [fftw_plan_dft], [will affect ⎕FFT])

apl_FFT=$ac_cv_lib_fftw3_fftw_plan_dft
} }
dash_test_FFT   # set apl_FFT to yes or no.
