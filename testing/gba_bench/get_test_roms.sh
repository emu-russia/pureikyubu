#!/bin/bash
# Fetch the public test ROMs the harness can be pointed at.
#
# `jsmolka/gba-tests` is a suite of MIT-licensed GBA test ROMs (ARM, Thumb, memory). Each image
# runs on the console and reports its result on screen, so running one through the harness and
# dumping the last frame as a PNG is a real end-to-end check of the CPU and the memory bus.
#
# The binaries are NOT part of the repository (they are somebody else's build output, and they are
# large); they land in testing/gba_bench/roms/, which is git-ignored.
#
#   testing/gba_bench/get_test_roms.sh
#   testing/gba_bench/check.sh --run testing/gba_bench/roms/arm.gba --frames 120 --png /tmp/gba_arm
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/roms"
BASE="https://github.com/jsmolka/gba-tests/raw/master"

mkdir -p "$OUT"

for rom in arm/arm.gba thumb/thumb.gba memory/memory.gba; do
    name="$(basename "$rom")"
    if [ -f "$OUT/$name" ]; then
        echo "have $name"
        continue
    fi
    echo "fetching $name"
    curl -sSL "$BASE/$rom" -o "$OUT/$name"
done

ls -l "$OUT"
