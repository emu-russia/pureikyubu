#!/usr/bin/env python3
"""Which files of the disc the frames of a capture read.

    python discmap.py capture.json "D:\\Isos\\image.iso" [--frames N]

The capture records, for every frame, the byte range the drive read (`disc.first` ..
`disc.last`). This resolves those ranges against the image's own file system table, which is how
a capture is turned into "the title was reading /Video/00_first_start.thp from frame 87".
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from isofst import Disc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('capture')
    ap.add_argument('image')
    ap.add_argument('--frames', type=int, default=0, help='only the first N frames')
    ap.add_argument('--rows', type=int, default=40)
    args = ap.parse_args()

    doc = json.load(open(args.capture, 'r', encoding='utf-8'))
    disc = Disc(args.image)

    files = sorted([e for e in disc.walk() if not e['dir']], key=lambda e: e['offset'])

    def owner(offset):
        # The last file that starts at or before the offset (the read may run past its end).
        lo, hi = 0, len(files) - 1
        best = None
        while lo <= hi:
            mid = (lo + hi) // 2
            if files[mid]['offset'] <= offset:
                best = files[mid]
                lo = mid + 1
            else:
                hi = mid - 1
        if best is None:
            return '<before the first file>', offset
        return best['path'], offset - best['offset']

    frames = doc['frames']
    if args.frames:
        frames = frames[:args.frames]

    print('%-6s %-10s %-12s %-12s %s' % ('frame', 'bytes', 'first', 'last', 'file'))
    rows = 0
    current = None
    run_start = None
    run_bytes = 0

    for f in frames:
        d = f['disc']
        if d['bytes'] == 0:
            continue
        path, rel = owner(d['first'])
        if path != current:
            if current is not None:
                print('  -> %s: frames %d..%d, %d bytes' % (current, run_start, f['i'] - 1, run_bytes))
            current = path
            run_start = f['i']
            run_bytes = 0
        run_bytes += d['bytes']

        if rows < args.rows:
            print('%-6d %-10d 0x%-10X 0x%-10X %s (+0x%X)' % (f['i'], d['bytes'], d['first'], d['last'], path, rel))
            rows += 1

    if current is not None:
        print('  -> %s: frames %d..%d, %d bytes' % (current, run_start, frames[-1]['i'], run_bytes))


if __name__ == '__main__':
    main()
