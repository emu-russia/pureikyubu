#!/usr/bin/env python3
"""Symbolize the guest PCs that a pureikyubu log mentions, using a CodeWarrior map file.

The emulator writes the current guest PC into its interrupt lines (`pc: 8000A73C`). With the
demo's `.map` (the SDK ships one next to every ELF), those addresses can be turned back into
function names, which shows where a demo spends its time - and, for a demo that never draws,
where it is stuck.

Usage: symbolize.py <demo.map> <log>
"""
import re
import sys


def load_map(path):
    """Return a sorted [(start, end, name)] list from a CodeWarrior map."""
    syms = []
    #   00002494 000080 80007994  4 OSReport 	os.a OSError.c
    pat = re.compile(r"^\s+([0-9a-f]{1,8})\s+([0-9a-f]{1,8})\s+([0-9a-f]{1,8})\s+(\d+)\s+(\S+)")
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = pat.match(line)
            if not m:
                continue
            _off, size, addr, _level, name = m.groups()
            start = int(addr, 16)
            syms.append((start, start + int(size, 16), name))
    syms.sort()
    return syms


def lookup(syms, pc):
    lo, hi = 0, len(syms) - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        start, end, name = syms[mid]
        if pc < start:
            hi = mid - 1
        elif pc >= end:
            lo = mid + 1
        else:
            return name
        if start <= pc:
            best = syms[mid]
    if best and best[0] <= pc < best[1]:
        return best[2]
    return "?"


def main():
    map_path, log_path = sys.argv[1], sys.argv[2]
    syms = load_map(map_path)
    counts = {}
    pat = re.compile(r"pc:\s*([0-9A-Fa-f]{8})")
    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = pat.search(line)
            if m:
                pc = int(m.group(1), 16)
                name = lookup(syms, pc)
                counts[name] = counts.get(name, 0) + 1
    for name, n in sorted(counts.items(), key=lambda kv: -kv[1])[:25]:
        print(f"{n:>8}  {name}")


if __name__ == "__main__":
    main()
