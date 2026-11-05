#!/bin/bash
#
# verify_TM3_exit.sh -- regression test for the --TM 3 abnormal-exit-code
# guarantee (see Error_macros.hh's fixme_exit_code(), IO_Files.hh's
# is_stop_after_file_error(), and Quad_FIO's TM3_trigger).
#
# GNU APL guarantees that under --TM 3, an abnormal condition (a segfault,
# or an internal FIXME/"this should never happen" trap) makes the
# interpreter process exit non-zero -- not just print a diagnostic and
# report false success via exit(0). This is what lets external tooling
# (src/Automated_Test_Report.sh) tell a genuine crash apart from a clean pass when it
# pipes a trailing ')OFF' to make the otherwise-still-alive --TM 3
# interpreter terminate.
#
# Exercising a real crash from ordinary APL syntax isn't possible (the
# FIXME/segfault trigger points are internal "should never happen" traps,
# not reachable from valid APL) and hard-coding one into the source (as
# an earlier iteration of this test did, via a throwaway standalone C++
# harness) is too heavy for a routine regression check. Instead, ⎕FIO has
# a small set of dedicated, otherwise-inert test hooks (¯19/¯6/¯7/¯20)
# that only actually do anything once armed via an explicit, one-shot
# magic-value trigger -- safe to call from real APL, and safe to leave
# in the tree without risking an accidental crash for ordinary ⎕FIO use.
#
# Usage: ./verify_TM3_exit.sh   (from the src/ directory, after 'make')

set -u

if [[ ! -x ./apl ]]; then
   echo "verify_TM3_exit.sh: must be run from the src/ directory" \
        "(need ./apl here)" >&2
   exit 1
fi

TRIGGER_ARMED=1095912753   # Quad_FIO::TM3_TRIGGER_ARMED, i.e. 0x41524D31 ("ARM1")
fail=0

# run_apl SCRIPT_TEXT --TM3?
#
# runs apl on the given inline APL text (piping ')OFF' so a disarmed --TM 3
# run terminates instead of sitting at the interactive prompt) and prints
# its exit code.
run_apl_exit_code()
{
   local script=$1
   local tm3=$2   # "yes" or "no"
   local run_home
   run_home=$(mktemp -d)
   local args=(-q -s)
   [[ $tm3 == yes ]] && args+=(--TM 3)

   HOME="$run_home" ./apl "${args[@]}" --eval "$script" \
      <<< ')OFF' > /dev/null 2>&1
   local rc=$?
   rm -rf "$run_home"
   echo "$rc"
}

# check DESCRIPTION SCRIPT_TEXT TM3? EXPECT_NONZERO?
check()
{
   local desc=$1 script=$2 tm3=$3 expect_nonzero=$4
   local rc
   rc=$(run_apl_exit_code "$script" "$tm3")

   local ok
   if [[ $expect_nonzero == yes ]]; then
      [[ $rc -ne 0 ]] && ok=yes || ok=no
   else
      [[ $rc -eq 0 ]] && ok=yes || ok=no
   fi

   if [[ $ok == yes ]]; then
      echo "PASS: $desc (exit=$rc)"
   else
      echo "FAIL: $desc (exit=$rc, expected" \
           "$([[ $expect_nonzero == yes ]] && echo non-zero || echo 0))"
      fail=1
   fi
}

echo "== disarmed (default state): every guarded ⎕FIO hacker function" \
     "must be a harmless no-op, regardless of --TM 3"
check "disarmed segfault (¯6), --TM 3"  "⎕FIO ¯6"  yes no
check "disarmed segfault (¯7), --TM 3"  "⎕FIO ¯7"  yes no
check "disarmed FIXME (¯20), --TM 3"    "⎕FIO ¯20" yes no
check "wrong magic value does not arm"  "42 ⎕FIO ¯19 ⋄ ⎕FIO ¯20" yes no

echo
echo "== armed, no --TM 3: must still exit 0 (unchanged default behaviour)"
check "armed FIXME (¯20), no --TM 3" \
      "$TRIGGER_ARMED ⎕FIO ¯19 ⋄ ⎕FIO ¯20" no no

echo
echo "== armed + --TM 3: must exit non-zero"
check "armed FIXME (¯20), --TM 3" \
      "$TRIGGER_ARMED ⎕FIO ¯19 ⋄ ⎕FIO ¯20" yes yes
check "armed segfault (¯7), --TM 3" \
      "$TRIGGER_ARMED ⎕FIO ¯19 ⋄ ⎕FIO ¯7" yes yes

echo
if (( fail )); then
   echo "verify_TM3_exit.sh: FAILED"
   exit 1
fi
echo "verify_TM3_exit.sh: all checks passed"
