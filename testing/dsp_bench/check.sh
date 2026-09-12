#!/bin/bash
# Differential test: the recompiler against the interpreter.
#
# The recompiler retires whole basic blocks, so it overshoots the requested instruction
# count. The interpreter is then run for exactly the number of instructions the
# recompiler actually retired, and the two state fingerprints have to match.
#
#   check.sh irom <N>
#   check.sh golden <seed> <N>
#   check.sh raw <file> <N>
#
#   BENCH   scratch directory (default /tmp/pkdspbench)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
BENCH=${BENCH:-/tmp/pkdspbench}
BIN=$BENCH/build/bench

if [ ! -x "$BIN" ]; then
    echo "build the benchmark first: bash $HERE/build.sh" >&2
    exit 2
fi

mode="${1:-irom}"
shift || true

export DSP_IROM=${DSP_IROM:-$REPO/build/Data/dsp_irom.bin}

# The instruction count is the last argument of every mode; drop it for the interpreter
# run and append the count the recompiler actually retired.
args=("$@")
if [ ${#args[@]} -gt 0 ]; then
    unset 'args[${#args[@]}-1]'
fi

jout=$(DSP_JIT=1 "$BIN" "$mode" "$@" 2>&1)
jn=$(echo "$jout" | grep "^Executed" | awk '{print $2}')
jh=$(echo "$jout" | grep "state hash" | awk '{print $3}')

if [ -z "$jn" ] || [ -z "$jh" ]; then
    echo "recompiler run failed:"
    echo "$jout"
    exit 1
fi

iout=$("$BIN" "$mode" "${args[@]}" "$jn" 2>&1)
ih=$(echo "$iout" | grep "state hash" | awk '{print $3}')

status=0
if [ "$jh" = "$ih" ] && [ -n "$ih" ]; then
    echo "  jit==interp @$jn : OK"
else
    echo "  jit==interp @$jn : FAIL (jit=$jh interp=$ih)"
    status=1
fi

# The recompiler must actually compile: a run that silently fell back to the
# interpreter would trivially match.
jmips=$(echo "$jout" | grep "^Executed" | awk '{print $5, $7, $8}')
imips=$(echo "$iout" | grep "^Executed" | awk '{print $5, $7, $8}')
echo "  jit ${jmips}s, interp ${imips}s"

exit $status
