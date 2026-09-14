#!/bin/bash
# Build the GBA harness and run it.
#
#   testing/gba_bench/check.sh                  # all unit tests
#   testing/gba_bench/check.sh Ppu              # only the Ppu* tests
#   testing/gba_bench/check.sh --bootrom --frames 300 --png /tmp/gba_shots
#
# GBA_BENCH selects the scratch directory (default /tmp/gbabench), OPT the compiler flags.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BLD=${GBA_BENCH:-/tmp/gbabench}

"$HERE/build.sh"

exec "$BLD/gba_test" "$@"
