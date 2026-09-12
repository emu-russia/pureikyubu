#!/bin/bash
# Build the reference benchmark from git HEAD (the pre-optimisation interpreter).
# It is the differential oracle for check.sh.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
BENCH=${BENCH:-/tmp/pkbench}
BASE=$BENCH/base
rm -rf $BASE
mkdir -p $BASE/src
# The reference is the same harness built from git HEAD, so the file set and the flags have to
# match build.sh exactly (the core sources and the headers they include are versioned together).

for f in gekko.cpp gekkoc.cpp gekkodec.cpp gekkodisasm.cpp gekkojit.cpp gekkojit_ps.cpp \
         gekkojit_x64.h gekkojit_ps.h gqr.h gekko.h gekkoc.h gekkodec.h gekkodisasm.h gekkojit.h; do
    (cd $REPO && git show HEAD:src/$f) > $BASE/src/$f
done

g++ -std=c++17 -D_LINUX -DBENCH_WITH_JIT -O2 -fno-strict-aliasing -w \
    -I$HERE -I$BASE/src \
    $BASE/src/gekko.cpp $BASE/src/gekkoc.cpp $BASE/src/gekkodec.cpp $BASE/src/gekkodisasm.cpp \
    $BASE/src/gekkojit.cpp $BASE/src/gekkojit_ps.cpp \
    $HERE/stubs.cpp $HERE/bench.cpp $HERE/prof.cpp \
    -o $BASE/bench -lpthread

echo "reference built"
