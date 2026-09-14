#!/bin/bash
# Fetch the public test ROMs the harness can be pointed at.
#
# `jsmolka/gba-tests` is a suite of MIT-licensed GBA test ROMs (ARM, Thumb, memory). Each image
# runs on the console and reports its result on screen, so running one through the harness and
# dumping the last frame as a PNG is a real end-to-end check of the CPU and the memory bus.
#
# `mattcurrie/dmg-acid2` and `mattcurrie/cgb-acid2` are the Acid2 pictures of the two Game Boy LCD
# controllers: they draw a face that is only correct when the background, the window, the objects,
# their priorities and (on a CGB) the colour palette registers all behave, so they are the
# end-to-end check of the Game Boy machine's picture.
#
# The binaries are NOT part of the repository (they are somebody else's build output, and they are
# large); they land in testing/gba_bench/roms/, which is git-ignored.
#
#   testing/gba_bench/get_test_roms.sh
#   testing/gba_bench/check.sh --run testing/gba_bench/roms/arm.gba --frames 120 --png /tmp/gba_arm
#   testing/gba_bench/check.sh --run testing/gba_bench/roms/cgb-acid2.gbc --gb --frames 300 --png /tmp/acid
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/roms"
BASE="https://github.com/jsmolka/gba-tests/raw/master"

mkdir -p "$OUT"

# The GBA suite: one file per test inside the repository.
for rom in arm/arm.gba thumb/thumb.gba memory/memory.gba; do
    name="$(basename "$rom")"
    if [ -f "$OUT/$name" ]; then
        echo "have $name"
        continue
    fi
    echo "fetching $name"
    curl -sSL "$BASE/$rom" -o "$OUT/$name"
done

# The two Acid2 images: they are release assets, one file each.
fetch_asset() {
    repo="$1"
    tag="$2"
    file="$3"
    if [ -f "$OUT/$file" ]; then
        echo "have $file"
        return
    fi
    echo "fetching $file"
    curl -sSL "https://github.com/$repo/releases/download/$tag/$file" -o "$OUT/$file"
}

fetch_asset mattcurrie/dmg-acid2 v1.0 dmg-acid2.gb
fetch_asset mattcurrie/cgb-acid2 v1.1 cgb-acid2.gbc

ls -l "$OUT"
