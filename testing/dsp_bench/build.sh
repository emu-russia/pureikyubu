#!/bin/bash
# Build the standalone DSPcore benchmark from the current working tree.
#
#   BENCH   scratch directory (default /tmp/pkdspbench); the binary lands in $BENCH/build
#   OPT     extra compiler flags (default -O2)
#
# Add -DDSP_JIT_TEST_WIN64_SHADOW to OPT for the host-ABI regression build: the shared
# emitter (src/jit_x64.h) then poisons the Win64 shadow space before every helper call,
# so a block that keeps live values there fails on Linux instead of only on Windows.
#
# Add -DDSP_JIT_DISABLED to OPT to build the interpreter-only configuration.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
BENCH=${BENCH:-/tmp/pkdspbench}
BLD=$BENCH/build
OPT="${OPT:--O2}"

rm -rf $BLD/src
mkdir -p $BLD/src

# The sources are compiled from a scratch copy: a quoted #include "pch.h" is resolved
# against the directory of the including file first, and the copy has no pch.h, so the
# stub in this directory wins over src/pch.h (which needs SDL / GL / ImGui).
cp $REPO/src/dsp.cpp $REPO/src/dspcore.cpp $REPO/src/dspdec.cpp $REPO/src/dspdma.cpp \
   $REPO/src/dsparam.cpp $REPO/src/dspjit_x64.cpp $REPO/src/dspjit_x86.cpp \
   $REPO/src/dsp.h $REPO/src/dspcore.h $REPO/src/dspdec.h $REPO/src/dspdma.h \
   $REPO/src/dsparam.h $REPO/src/dspai.h $REPO/src/dspjit.h \
   $REPO/src/jit_x64.h $REPO/src/jit_x86.h \
   $BLD/src/

g++ -std=c++17 -D_LINUX $OPT -fno-strict-aliasing -w \
    -I$BLD/src -I$HERE -I$REPO/src -I$REPO/testing \
    $BLD/src/dsp.cpp $BLD/src/dspcore.cpp $BLD/src/dspdec.cpp \
    $BLD/src/dspdma.cpp $BLD/src/dsparam.cpp \
    $BLD/src/dspjit_x64.cpp $BLD/src/dspjit_x86.cpp \
    $HERE/stubs.cpp $HERE/bench.cpp \
    -o $BLD/bench -lpthread

echo "built $BLD/bench ($OPT)"
