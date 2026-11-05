#!/bin/bash
#
# verify_tcp_port.sh -- regression test for --tcp_port range validation
# (Bugs28 #100(v)).
#
# An out-of-range --tcp_port value (0, negative, or > 65535) used to be
# accepted silently and truncated to 16 bits by htons() in main.cc, so
# e.g. --tcp_port 99999 actually listened on 99999 mod 65536 = 34463 --
# a confusing, unannounced substitution instead of a clear rejection.
# UserPreferences.cc now rejects it immediately with a message on
# stderr and a non-zero exit code, before any socket is ever touched.
#
# A *valid* --tcp_port blocks waiting for a client connection, so this
# script only exercises the rejection path (immediate, non-blocking).
#
# Usage: ./verify_tcp_port.sh   (from the src/ directory, after 'make')

set -u

if [[ ! -x ./apl ]]; then
   echo "verify_tcp_port.sh: must be run from the src/ directory" \
        "(need ./apl here)" >&2
   exit 1
fi

fail=0

# check DESCRIPTION PORT
#
# runs apl with --tcp_port PORT and expects an immediate non-zero exit
# plus an "invalid port number" message on stderr.
check()
{
   local desc=$1 port=$2
   local run_home
   run_home=$(mktemp -d)
   local out
   out=$(HOME="$run_home" timeout 5 ./apl --script --tcp_port "$port" 2>&1)
   local rc=$?
   rm -rf "$run_home"

   if [[ $rc -ne 0 && $out == *"invalid port number"* ]]; then
      echo "PASS: $desc (exit=$rc)"
   else
      echo "FAIL: $desc (exit=$rc, output: $out)"
      fail=1
   fi
}

echo "== out-of-range --tcp_port values must be rejected immediately"
check "port 0"      0
check "negative"    -5
check "too large"   99999
check "just over"   65536

echo
if (( fail )); then
   echo "verify_tcp_port.sh: FAILED"
   exit 1
fi
echo "verify_tcp_port.sh: all checks passed"
