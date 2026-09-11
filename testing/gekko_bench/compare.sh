#!/bin/bash
# Throughput of the reference interpreter (git HEAD), the current interpreter and
# the current recompiler, for every workload.
BENCH=${BENCH:-/tmp/pkbench}
N=${1:-30000000}
DOL=${2:-$(cd "$(dirname "$0")/../.." && pwd)/build/pong.dol}

best() {	# best of three runs of "$@"
    local b=0 v
    for i in 1 2 3; do
        v=$("$@" 2>/dev/null | grep -oE "[0-9.]+ MIPS" | cut -d' ' -f1)
        b=$(echo "$v $b" | awk '{print ($1>$2)?$1:$2}')
    done
    echo "$b"
}

printf "%-18s %9s %9s %9s %9s\n" workload base interp jit jit/base
for w in workload.bin workload_alu.bin workload_mem.bin w_win16.bin w_alu100.bin w_alu16000.bin; do
    [ -f "$BENCH/$w" ] || continue
    b=$(best env BENCH_RAW=$BENCH/$w $BENCH/base/bench dummy $N 1000000)
    c=$(best env BENCH_RAW=$BENCH/$w $BENCH/build/bench dummy $N 1000000)
    j=$(best env BENCH_JIT=1 BENCH_RAW=$BENCH/$w $BENCH/build/bench dummy $N 1000000)
    printf "%-18s %9s %9s %9s %8sx\n" "$w" "$b" "$c" "$j" "$(echo "$j/$b" | bc -l | cut -c1-5)"
done
b=$(best $BENCH/base/bench $DOL 3000000)
c=$(best $BENCH/build/bench $DOL 3000000)
j=$(best env BENCH_JIT=1 $BENCH/build/bench $DOL 3000000)
printf "%-18s %9s %9s %9s %8sx\n" "$(basename $DOL)" "$b" "$c" "$j" "$(echo "$j/$b" | bc -l | cut -c1-5)"
