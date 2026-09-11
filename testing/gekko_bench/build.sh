#!/bin/bash
# Build the standalone Gekko benchmark from the current working tree.
#
#   BENCH   scratch directory (default /tmp/pkbench); the binaries land in $BENCH/build
#   OPT     extra compiler flags (default -O2)
#
# Add -DGEKKO_JIT_TEST_WIN64_SHADOW to OPT for the host-ABI regression build: the
# emitter then poisons the Win64 shadow space before every helper call, so a block
# that keeps live values there fails here instead of only on Windows.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
BENCH=${BENCH:-/tmp/pkbench}
BLD=$BENCH/build
OPT="${OPT:--O2}"

rm -rf $BLD/src
mkdir -p $BLD/src
cp $REPO/src/gekko.cpp $REPO/src/gekkoc.cpp $REPO/src/gekkodec.cpp \
   $REPO/src/gekkodisasm.cpp $REPO/src/gekkojit.cpp $REPO/src/gqr.h $BLD/src/

g++ -std=c++17 -D_LINUX -DBENCH_WITH_JIT $OPT -fno-strict-aliasing -w \
    -I$HERE -I$BLD/src -I$REPO/src \
    $BLD/src/gekko.cpp $BLD/src/gekkoc.cpp $BLD/src/gekkodec.cpp \
    $BLD/src/gekkodisasm.cpp $BLD/src/gekkojit.cpp \
    $HERE/stubs.cpp $HERE/bench.cpp $HERE/prof.cpp \
    -o $BLD/bench -lpthread

echo "built $BLD/bench ($OPT)"
