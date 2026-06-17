
# compute the GNU APL build date, using $SOURCE_DATE_EPOCH if present.
# if $SOURCE_DATE_EPOCH is set, then this is supposedly a reproducible build
# (see https://reproducible-builds.org/docs/source-date-epoch ).
# Reproducible builds should not contain any non-predictable data.i
# In our case the non-predictable data is apl_BUILD_DATE and apl_BUILD_OS.
#

# format the date, see https://unix.stackexchange.com/questions/230464/how-to-format-date-output-with-spaces-as-variable-in-script
#
dash_format_date()
{
  date "+%F %R:%S %Z" "$@"
}

dash_build_date()
{ {

DATE_FMT="+%F\ %R:%S\ %Z"
apl_BUILD_DATE=$(dash_format_date)      # current time (not predictable)
apl_BUILD_OS=$(uname -s -r -m)          # current OS

AS_IF( [test -n "$SOURCE_DATE_EPOCH"],
       [ apl_BUILD_OS="none"
         apl_BUILD_DATE=             $(
                   dash_format_date -u -d "$SOURCE_DATE_EPOCH" 2>/dev/null ||
                   dash_format_date -u -r $SOURCE_DATE_EPOCH  ||
                   dash_format_date -u)
       ])
AC_SUBST([apl_BUILD_DATE])
AC_SUBST([apl_BUILD_OS])
} }

dash_build_date     # set apl_BUILD_DATE and apl_BUILD_OS
