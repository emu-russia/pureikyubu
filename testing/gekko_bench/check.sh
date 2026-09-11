#!/bin/bash
# Compare the state produced by the reference (git HEAD) interpreter, the current
# interpreter and the JIT. The JIT runs whole basic blocks, so it overshoots the
# requested instruction count; the interpreter is then run for the exact number of
# instructions the JIT retired.
BENCH=${BENCH:-/tmp/pkbench}
WL=${1:-/tmp/pkbench/workload.bin}
N=${2:-20000000}

# 1. reference (pre-optimisation interpreter) vs current interpreter at N instructions
r=$(BENCH_RAW=$WL $BENCH/base/bench dummy $N 1000000 2>&1 | grep "state hash" | awk '{print $3}')
c=$(BENCH_RAW=$WL $BENCH/build/bench dummy $N 1000000 2>&1 | grep "state hash" | awk '{print $3}')

status=0
if [ "$r" == "$c" ]; then
    echo "  interp ref==cur : OK"
else
    echo "  interp ref==cur : FAIL ($r vs $c)"
    status=1
fi

# 2. JIT vs current interpreter at the same retired instruction count
jout=$(BENCH_JIT=1 BENCH_RAW=$WL $BENCH/build/bench dummy $N 1000000 2>&1)
jn=$(echo "$jout" | grep "^Executed" | sed 's/Executed \([0-9]*\) instructions.*/\1/')
jh=$(echo "$jout" | grep "state hash" | awk '{print $3}')
ch=$(BENCH_RAW=$WL $BENCH/build/bench dummy $jn 1000000 2>&1 | grep "state hash" | awk '{print $3}')

if [ "$jh" == "$ch" ] && [ -n "$jh" ]; then
    echo "  jit==interp @$jn : OK"
else
    echo "  jit==interp @$jn : FAIL (jit=$jh interp=$ch)"
    status=1
fi

exit $status
