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
for f in gekko.cpp gekkoc.cpp gekkodec.cpp gekkodisasm.cpp gqr.h gekko.h gekkoc.h gekkodec.h gekkodisasm.h; do
    (cd $REPO && git show HEAD:src/$f) > $BASE/src/$f
done

g++ -std=c++17 -D_LINUX -O2 -fno-strict-aliasing -w \
    -I$HERE -I$BASE/src \
    $BASE/src/gekko.cpp $BASE/src/gekkoc.cpp $BASE/src/gekkodec.cpp $BASE/src/gekkodisasm.cpp \
    $HERE/stubs.cpp $HERE/bench.cpp $HERE/prof.cpp \
    -o $BASE/bench -lpthread

echo "reference built"
