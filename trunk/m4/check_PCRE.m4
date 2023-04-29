
# check if libpcre is installed and usable

# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_PCRE()
{ {
local ac_cv_lib_pcre2_32_pcre2_compile_32
apl_PCRE=no

m4_include([m4/ax_path_lib_pcre.m4])   # define AX_PATH_LIB_PCRE()
AX_PATH_LIB_PCRE([])                   # set ac_cv_lib_pcre2_32_pcre2_compile_32
apl_PCRE=$ac_cv_lib_pcre2_32_pcre2_compile_32

# export apl_PCRE to Makefile.am
AM_CONDITIONAL(apl_PCRE, apl_YES($apl_PCRE))
} }
dash_test_PCRE   # set apl_PCRE to yes or no.
