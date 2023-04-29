
# check if libfftw3 is installed and usable

# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_FFT()
{ {
local ac_cv_lib_fftw3_fftw_plan_dft
apl_FFT=no   # assume error

AC_CHECK_HEADER([fftw3.h], , return)

apl_OPT_LIB([fftw3], [fftw_plan_dft], [will affect ⎕FFT])

apl_FFT=$ac_cv_lib_fftw3_fftw_plan_dft
} }
dash_test_FFT   # set apl_FFT to yes or no.
