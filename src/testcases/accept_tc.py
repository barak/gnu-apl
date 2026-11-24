#!/usr/bin/python3

# vim: et:ts=4:sw=4

"""
    usage example (from the src/ directory):

  testcases/accept_tc.py testsuites/main/Quad_CR10.tc

  reads testsuites/main/Quad_CR10.tc.log (written next to the testcase when
  it was run) and rewrites testsuites/main/Quad_CR10.tc so that it expects
  the output in the .log file. The .tc.log file can be given instead of
  the .tc file.
"""

import os
import sys

# some .tc.log transcripts contain surrogate-escaped bytes (see the
# surrogateescape comment below); diagnostic print()s of those values
# would otherwise crash with UnicodeEncodeError: surrogates not allowed.
sys.stdout.reconfigure(errors='backslashreplace')

# for every testcase xxx.tc in $*, adjust xxx.tc in such a way that
# it produces xxx.tc.log.

def wildcard_matches(apl, ref):
    """Python port of DiffOut::different() (src/DiffOut.cc): returns True
    if the actual output 'apl' still matches the (possibly wildcarded)
    old expected line 'ref', i.e. the two are NOT different. Used to
    decide whether a wildcard in the previous expected output still
    applies to the new actual output, so accept_tc.py can preserve it
    instead of overwriting it with a literal value (see ch03's "Wildcards
    in Expected Output" section for why this previously had to be done
    by hand: a diff mismatch here is a strong majority of real testcase
    regressions, only never one that a wildcard character itself
    causes -- so preserving a wildcard that still matches is safe).
    """
    ai = ri = 0
    while True:
        a = apl[ai] if ai < len(apl) else '\0'
        r = ref[ri] if ri < len(ref) else '\0'
        ai += 1
        ri += 1

        if a == r:
            if a == '\0':
                return True   # end of both strings: match
            continue

        if r == '⁰':   # one or more digits
            if not a.isdigit():
                return False
            while ai < len(apl) and apl[ai].isdigit():
                ai += 1
            continue

        if r == '¹':   # zero or more spaces
            if a != ' ':
                return False
            while ai < len(apl) and apl[ai] == ' ':
                ai += 1
            continue

        if r == '²':   # a floating-point number
            if not (a.isdigit() or a in '¯.EJ'):
                return False
            while True:
                if ai < len(apl) and (apl[ai].isdigit() or apl[ai] in '.EJ'):
                    ai += 1
                elif ai < len(apl) and apl[ai] == '¯':
                    ai += 1
                else:
                    break
            continue

        if r == '³':   # the rest of the line: anything
            return True

        if r == '⁴':   # an optional overbar
            if a != '¯':
                ai -= 1   # a was not the overbar: put it back
            continue

        if r == '⁵':   # + or -
            if a not in ('+', '-'):
                return False
            continue

        if r == '⁶':   # 0 or 1, depending on RATIONAL_NUMBERS_WANTED
            # accept_tc.py doesn't know the build this .tc.log came
            # from; either digit is accepted as a safe superset of the
            # single value the actual build's DiffOut::different() would
            # have required.
            if a not in ('0', '1'):
                return False
            continue

        if r == 'ⁿ':   # an optional SI multiplier: m, n, u, or μ
            if a in ('m', 'n', 'u', 'μ'):
                continue
            ai -= 1   # a was not a multiplier: put it back
            continue

        return False   # r does not match a: different


for tcfile_name in sys.argv[1:]:
    if tcfile_name.endswith(".tc.log"):   # the .log file was given
        tcfile_name = tcfile_name[:-4]
    logfile_name = "%s.log" % tcfile_name
    outfile_name = "%s.new" % tcfile_name
    print("fixing %s from %s..." % (tcfile_name, logfile_name))
    # surrogateescape: some .tc.log transcripts contain byte sequences
    # that aren't valid UTF-8 (e.g. ⎕UCS output for out-of-range
    # codepoints, using pre-RFC3629 extended UTF-8) -- tolerate them
    # losslessly instead of raising UnicodeDecodeError.
    log_file = open(logfile_name, 'r', errors='surrogateescape')
    out_file = open(outfile_name, 'w', errors='surrogateescape')

    # The previous (soon to be overwritten) .tc file's lines, consulted
    # positionally below so that ANY emitted line -- not just one that
    # happened to be reported as a diff in this particular run -- can
    # keep its old text when that old text still matches the new actual
    # output (literally, or via a wildcard: wildcard_matches() treats a
    # non-wildcard old line as requiring an exact match, so this is a
    # strict generalization, never a loosening). Without this, every
    # wildcard in the file that wasn't ALSO failing right now -- the
    # normal case, since matching is the whole point of a wildcard --
    # got silently overwritten with today's literal value (timestamps,
    # dates, elided paths, ...) on every single regeneration.
    try:
        orig_lines = open(tcfile_name, 'r', errors='surrogateescape').read().split('\n')
    except FileNotFoundError:
        orig_lines = []
    orig_idx = 0
    kept = 0

    def emit(text):
        global orig_idx, kept
        if orig_idx < len(orig_lines):
            orig = orig_lines[orig_idx]
            orig_idx += 1
            if orig != text and wildcard_matches(text, orig):
                kept += 1
                text = orig
        print(text, file=out_file)

    lines = log_file.readlines()
    reused = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        if line[:6] == "----> ":        # APL input
            apl_inp = line[6:-1]
            print("input='%s'" % apl_inp)
            emit(apl_inp)
        elif line[:3] == "== ":        # common output
            apl_com = line[3:-1]
            emit(apl_com)
            print("common='%s'" % apl_com)
        elif line[:8] == "apl: ⋅⋅⋅":   # apl output
            apl_out = line[8:-4]
            # the next line, if any, is the old expected ("ref:") line
            # this apl output was diffed against -- if the old line's
            # wildcards still match the new output, keep the old
            # (wildcarded) line instead of the new literal one. (emit()
            # below re-checks the same thing against the .tc file's own
            # line, so this is mostly belt-and-braces / accurate log
            # messages, but keeps the "reused" counter meaningful.)
            next_line = lines[i + 1] if i + 1 < len(lines) else ""
            if next_line[:8] == "ref: ⋅⋅⋅":
                apl_ref = next_line[8:-4]
                i += 1   # consume the ref: line too
                if wildcard_matches(apl_out, apl_ref):
                    print("apl='%s' (kept wildcarded ref '%s')" % (apl_out, apl_ref))
                    reused += 1
                else:
                    print("apl='%s'" % apl_out)
            else:
                print("apl='%s'" % apl_out)
            emit(apl_out)
        elif line[:8] == "ref: ⋅⋅⋅":   # reference (not paired with a
                                        # preceding apl: line -- shouldn't
                                        # normally happen, but handled the
                                        # same as before just in case)
            apl_ref = line[8:-4]
            print("apl='%s'" % apl_ref)
        elif line[:8] == " ∆ : ⋅⋅⋅":   # delta
            apl_delta = line[8:-1]
            print("delta='%s'" % apl_delta)
        elif line[:6] == "-skp> ":   # a line that the run did not read
            # (]FILE_CTL SKIP_*, or after ]NEXTFILE or a --TM 5/6 stop):
            # keep it unchanged (it was deleted, Bugs40 #59)
            emit(line[6:-1])
        elif line[:7] == "extra: ":   # reference
            # the output starts after "extra: " (DiffOut.cc), not after
            # "extra:", which kept a leading blank (Bugs41 #92)
            apl_extra = line[7:-1]
            print("apl='%s'" % apl_extra)
            emit(apl_extra)
        else:
            print("other='%s'" % line)
        i += 1

    if reused:
        print("%d wildcarded line(s) preserved via the .tc.log diff" % reused)
    if kept:
        print("%d line(s) kept verbatim from the previous %s (still matched, "
              "wildcarded or not)" % (kept, tcfile_name))

    out_file.close()
    log_file.close()
    if os.access(tcfile_name, os.W_OK):     os.remove(tcfile_name)
    os.rename(outfile_name, tcfile_name)
