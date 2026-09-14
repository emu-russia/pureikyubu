#!/bin/bash
# Build the standalone GBA core harness (unit tests + headless ROM runner).
#
# The GBA module is self contained (no SDL, no OpenGL, no pch.h), so the harness is a plain g++
# build of src/gba plus testing/gba_bench. Nothing else from the repository is needed.
#
#   GBA_BENCH   scratch directory (default /tmp/gbabench); the binary lands in $GBA_BENCH/gba_test
#   OPT         extra compiler flags (default -O2)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
BLD=${GBA_BENCH:-/tmp/gbabench}
OPT="${OPT:--O2}"

mkdir -p $BLD

# gba_sdl.cpp is the SDL2 frontend of the emulator: it is not part of the standalone core (the
# core has no SDL dependency at all), so it is left out here.
CORE_SOURCES=$(ls $REPO/src/gba/*.cpp | grep -v '/gba_sdl\.cpp$')

g++ -std=c++17 $OPT -w -fno-strict-aliasing \
    -I$REPO/src/gba -I$HERE \
    $CORE_SOURCES \
    $HERE/test_*.cpp \
    -o $BLD/gba_test

echo "built $BLD/gba_test ($OPT)"
