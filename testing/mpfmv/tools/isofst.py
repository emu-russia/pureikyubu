#!/usr/bin/env python3
"""Minimal GameCube disc reader: dump the FST, locate and describe the THP movie files.

Usage:
    python isofst.py <image.iso> [--header] [--list] [--find thp]
                                [--thpinfo <fstpath>] [--extract <fstpath> <out>] [--byname]

The script understands a plain ISO/GCM (a raw 1:1 image); the RVZ/NKit containers the
emulator also opens are not handled. Only what the FMV research needs is implemented:
the boot block, the file system table and the THP stream header.

Layout notes (the same one src/dvd.cpp uses):
  * the boot block holds the FST offset at 0x424 and the FST length at 0x428;
  * an FST entry is 12 bytes: u8 isDir, u24 name offset, u32 file offset (a file) or
    the parent entry index (a directory), u32 file length (a file) or the index one past
    the directory's subtree (a directory);
  * everything after the first byte of an entry is big-endian on disc;
  * the name table starts right after the last entry and has no terminator of its own;
  * the root entry's `nextOffset` is the number of entries in the table.
"""

import argparse
import struct
import sys

NAME_TABLE_MIN = 0x10


class Disc:
    def __init__(self, path):
        self.f = open(path, 'rb')
        self.f.seek(0)
        boot = self.f.read(0x440)
        if len(boot) < 0x440:
            raise SystemExit('image is too small to hold a boot block')

        self.boot = boot
        self.game_code = boot[0:6].decode('ascii', 'replace')
        self.dol_offset = be32(boot, 0x420)
        self.fst_offset = be32(boot, 0x424)
        self.fst_size = be32(boot, 0x428)

        self.f.seek(self.fst_offset)
        fst = self.f.read(self.fst_size)
        if len(fst) < self.fst_size:
            raise SystemExit('the FST is truncated')
        self.fst = fst

        root = self.entry(0)
        if not root['is_dir'] or root['field3'] != 0 or root['name_off'] != 0 or root['field4'] < 1:
            raise SystemExit('the FST root entry does not validate: %r' % (root,))

        self.count = root['field4']
        if self.count * 12 > self.fst_size:
            raise SystemExit('the FST claims %d entries, which does not fit' % self.count)
        self.strings = self.fst[self.count * 12:]

    def entry(self, index):
        b = self.fst[index * 12:index * 12 + 12]
        if len(b) < 12:
            raise SystemExit('FST entry %d is past the end of the table' % index)
        return {
            'index': index,
            'is_dir': b[0] != 0,
            'name_off': int.from_bytes(b[1:4], 'big'),
            'field3': int.from_bytes(b[4:8], 'big'),
            'field4': int.from_bytes(b[8:12], 'big'),
        }

    def name(self, index):
        off = self.entry(index)['name_off']
        end = self.strings.find(b'\0', off)
        if end < 0:
            end = len(self.strings)
        return self.strings[off:end].decode('ascii', 'replace')

    def walk(self, index=1, end=None, prefix=''):
        """Depth-first walk. A directory spans index+1 .. its `field4` - 1."""
        if end is None:
            end = self.count
        out = []
        i = index
        while i < end:
            e = self.entry(i)
            name = self.name(i)
            path = prefix + '/' + name
            if e['is_dir']:
                out.append({'path': path, 'offset': None, 'size': None, 'dir': True})
                if e['field4'] > i:
                    out.extend(self.walk(i + 1, e['field4'], path))
                    i = e['field4']
                else:
                    i += 1
            else:
                out.append({'path': path, 'offset': e['field3'], 'size': e['field4'], 'dir': False})
                i += 1
        return out

    def read_file(self, path):
        for e in self.walk():
            if e['path'].lower() == path.lower() and not e['dir']:
                self.f.seek(e['offset'])
                return self.f.read(e['size'])
        raise SystemExit('%s is not in the FST' % path)

    def find(self, path):
        """The FST entry of a file, or None."""
        for e in self.walk():
            if e['path'].lower() == path.lower() and not e['dir']:
                return e
        return None


def be32(buf, off):
    return int.from_bytes(buf[off:off + 4], 'big')


def thp_info(data):
    """Decode the scalar part of the THP stream header.

    Only the fields the FMV research relies on are decoded; the component table that
    follows them has not been reverse engineered here (the emulator is the authority on
    what the guest does with it, and the geometry below is read from the video
    description block the header carries for the video component).
    """
    if data[0:3] != b'THP':
        return None

    version = be32(data, 4)
    max_buffer = be32(data, 8)
    max_audio = be32(data, 0x0C)
    fps = struct.unpack_from('>f', data, 0x10)[0]
    frames = be32(data, 0x14)
    first_frame_size = be32(data, 0x18)

    # The video description block: the width and the height sit at 0x46 and 0x4A, each in
    # the high half of its own word (`00 00 02 80` is 640, `00 00 01 E0` is 480).
    width = int.from_bytes(data[0x46:0x48], 'big') if len(data) > 0x50 else 0
    height = int.from_bytes(data[0x4A:0x4C], 'big') if len(data) > 0x50 else 0
    if not (16 <= width <= 4096 and 16 <= height <= 4096):
        width = height = 0

    return {
        'version': '0x%08X' % version,
        'max_buffer_size': max_buffer,
        'max_audio_samples': max_audio,
        'fps': fps,
        'frames': frames,
        'first_frame_size': first_frame_size,
        'width': width,
        'height': height,
        'duration_s': frames / fps if fps else 0.0,
        'audio': max_audio > 0,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('image')
    ap.add_argument('--header', action='store_true')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--find', default=None)
    ap.add_argument('--thpinfo', default=None)
    ap.add_argument('--allthp', action='store_true', help='decode the header of every .thp')
    ap.add_argument('--range', default=None,
                    help='print only <offset>:<length> of an FST path, for a capture script')
    ap.add_argument('--extract', nargs=2, metavar=('FSTPATH', 'OUT'))
    args = ap.parse_args()

    d = Disc(args.image)

    # `--range` is the scriptable form: nothing but the disc range, so that a shell can capture it.
    if args.range:
        e = d.find(args.range)
        if e is None:
            raise SystemExit('%s is not in the FST' % args.range)
        print('0x%X:%d' % (e['offset'], e['size']))
        return

    print('game code   : %s' % d.game_code)
    print('dol offset  : 0x%08X' % d.dol_offset)
    print('fst offset  : 0x%08X' % d.fst_offset)
    print('fst size    : 0x%X' % d.fst_size)
    print('fst entries : %d (%d bytes), name table %d bytes'
          % (d.count, d.count * 12, len(d.strings)))

    if args.header:
        print('\nboot block:')
        for off in range(0x400, 0x440, 16):
            print('  %04X  %s' % (off, ' '.join('%02X' % c for c in d.boot[off:off + 16])))

    entries = d.walk()
    files = [e for e in entries if not e['dir']]
    dirs = [e for e in entries if e['dir']]
    print('files       : %d in %d directories' % (len(files), len(dirs)))

    if args.list:
        for e in entries:
            if e['dir']:
                print('  %s/' % e['path'])
            else:
                print('  %-58s @0x%08X %10d' % (e['path'], e['offset'], e['size']))

    if args.find:
        needle = args.find.lower()
        hits = [e for e in entries if needle in e['path'].lower()]
        print('\n--- matches for %r: %d ---' % (args.find, len(hits)))
        for e in hits:
            if e['dir']:
                print('  %s/' % e['path'])
            else:
                print('  %-58s @0x%08X %10d bytes' % (e['path'], e['offset'], e['size']))

    def show_thp(path):
        entry = d.find(path)
        data = d.read_file(path)
        info = thp_info(data)
        print('\n--- %s (%d bytes @ 0x%X) ---' % (path, len(data), entry['offset']))
        if info is None:
            print('  not a THP stream (magic %r)' % data[0:4])
            return
        print('  %-18s %s' % ('disc offset', '0x%X' % entry['offset']))
        print('  %-18s %d' % ('disc length', entry['size']))
        for k in ('version', 'max_buffer_size', 'max_audio_samples', 'fps', 'frames',
                  'first_frame_size', 'width', 'height', 'duration_s', 'audio'):
            print('  %-18s %s' % (k, info[k]))

    if args.thpinfo:
        show_thp(args.thpinfo)

    if args.allthp:
        for e in entries:
            if not e['dir'] and e['path'].lower().endswith('.thp'):
                show_thp(e['path'])

    if args.extract:
        path, out = args.extract
        data = d.read_file(path)
        with open(out, 'wb') as f:
            f.write(data)
        print('wrote %s (%d bytes)' % (out, len(data)))


if __name__ == '__main__':
    main()
