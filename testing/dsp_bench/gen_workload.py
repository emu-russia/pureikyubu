#!/usr/bin/env python3
"""Generate a synthetic DSPcore workload for the standalone benchmark.

The workload is a stream of instruction words sampled from the hardware golden vector
table (testing/dsp_golden_alu_vectors.h). Those words were captured from the gate-level
DSP core, so every one of them is an instruction the real core executes - the generator
does not have to know the encodings, and it cannot emit an undefined opcode.

The words are written as big-endian 16-bit values, which is the byte order the DSP
instruction memory uses (DspCore::TranslateIMem hands the raw bytes to the decoder).

Usage:
    gen_workload.py /tmp/dsp.bin --words 3900 --seed 12345

Then:
    DSP_RAW=/tmp/dsp.bin bench raw /tmp/dsp.bin 2000000
    bash check.sh raw /tmp/dsp.bin 2000000
"""

import argparse
import os
import random
import re
import struct
import sys

MAX_IRAM_WORDS = 4096		# 8 KB of instruction RAM, one 16-bit word per slot


def parse_vectors(header_path):
    """Return the (word, word2) pairs of the golden vector table."""
    with open(header_path, "r", encoding="utf-8") as f:
        text = f.read()

    body = text.split("kAluVectors[]", 1)
    if len(body) != 2:
        raise SystemExit("kAluVectors[] not found in %s" % header_path)

    vectors = []
    pattern = re.compile(r"\{\s*0x([0-9A-Fa-f]+),\s*(\d+),\s*0x([0-9A-Fa-f]+),")
    for m in pattern.finditer(body[1].split("};", 1)[0]):
        word = int(m.group(1), 16)
        two_word = int(m.group(2))
        word2 = int(m.group(3), 16)
        vectors.append((word, word2 if two_word else None))

    if not vectors:
        raise SystemExit("no vectors parsed from %s" % header_path)

    return vectors


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    header = os.path.join(here, "..", "dsp_golden_alu_vectors.h")

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("output", help="output image (big-endian 16-bit words)")
    parser.add_argument("--words", type=int, default=MAX_IRAM_WORDS, help="how many halfwords to emit")
    parser.add_argument("--seed", type=int, default=12345, help="random seed")
    parser.add_argument("--header", default=header, help="path to dsp_golden_alu_vectors.h")
    parser.add_argument("--no-loop", dest="loop", action="store_false",
                        help="do not append the `jmp 0` self-loop (the run then stops at the end of the image)")
    args = parser.parse_args()

    vectors = parse_vectors(args.header)
    rng = random.Random(args.seed)

    words = []
    while len(words) < args.words:
        word, word2 = rng.choice(vectors)
        words.append(word)
        if word2 is not None:
            words.append(word2)

    # `jmp 0` (0x029F / 0x0000, the encoding the IROM's own main loop uses) makes the image
    # loop forever, so the benchmark can run for any number of instructions.
    if args.loop:
        words = words[:MAX_IRAM_WORDS - 2]
        words += [0x029F, 0x0000]

    with open(args.output, "wb") as f:
        for w in words:
            f.write(struct.pack(">H", w))

    print("wrote %s: %d halfwords, seed %d, from %d golden vectors" %
          (args.output, len(words), args.seed, len(vectors)))


if __name__ == "__main__":
    main()
