#!/usr/bin/env python3
"""Resolve the hot program counters of a capture against the emulator's own PC sampler.

    python pcsample.py capture.json pcsample.json

`guestprof` reports the guest's basic blocks in 4 KB buckets, which is fine for a chart but not
enough to name a function when the symbol map has a gap. The emulator's own sampling profiler
(`StartProfiler`, src/debug.cpp) reads `regs.pc` every couple of emulated milliseconds; this script
keeps the samples that fall inside the movie window of the capture and shows the exact addresses,
so the address a bucket stands for can be pinned down.

The sampler document is `{ "sampleData": [ticks, pc, ticks, pc, ...] }`.
"""

import argparse
import bisect
import collections
import json
import os
import sys

MPFMV = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(MPFMV))


def load_map(path):
    addrs, names = [], []
    if not path or not os.path.exists(path):
        return addrs, names
    for line in open(path, 'r', encoding='utf-8', errors='replace'):
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        try:
            addr = int(parts[0], 16)
        except ValueError:
            continue
        name = parts[1].strip()
        if name.startswith('['):
            continue
        addrs.append(addr)
        names.append(name)
    order = sorted(range(len(addrs)), key=lambda i: addrs[i])
    return [addrs[i] for i in order], [names[i] for i in order]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('capture')
    ap.add_argument('sample')
    ap.add_argument('--map', default=os.path.join(REPO, 'build', 'Data', 'GM8E.map'))
    ap.add_argument('--top', type=int, default=30)
    ap.add_argument('--all-frames', action='store_true', help='do not filter to the movie window')
    args = ap.parse_args()

    addrs, names = load_map(args.map)

    def nearest(pc, limit=0x8000):
        i = bisect.bisect_right(addrs, pc) - 1
        if i < 0 or pc - addrs[i] > limit:
            return ''
        return '%s+0x%X' % (names[i], pc - addrs[i])

    capture = json.load(open(args.capture, 'r', encoding='utf-8'))
    ticks_per_second = capture['ticksPerSecond'] or 1

    start = 0
    if not args.all_frames:
        for frame in capture['frames']:
            if frame['disc']['bytes'] > 0 and frame['movie']:
                start = frame['t']
                break

    flat = json.load(open(args.sample, 'r', encoding='utf-8'))['sampleData']

    histogram = collections.Counter()
    total = 0
    for i in range(0, len(flat) - 1, 2):
        if flat[i] >= start:
            histogram[flat[i + 1]] += 1
            total += 1

    print('window from tick %d (%.2f s emulated), %d samples' % (start, start / ticks_per_second, total))
    print()
    print('%-12s %8s %10s  %s' % ('pc', 'share', 'samples', 'nearest symbol'))
    for pc, count in histogram.most_common(args.top):
        print('  0x%08X %7.2f%% %10d  %s' % (pc, count * 100.0 / max(total, 1), count, nearest(pc)))

    print()
    print('%d distinct addresses' % len(histogram))


if __name__ == '__main__':
    main()
