#!/usr/bin/env python3
"""Summarize a DolphinSDK demo sweep (issue #385).

Reads the folder produced by sweep.ps1 (<demo>.log, <demo>.png, <demo>.txt) and prints a table
with the evidence that decides whether a demo did anything:

  pe      - "PE_FINISHINT asserted" lines: finished GX frames. A GX demo with 0 never drew.
  vi      - "VIINT asserted" lines: vertical interrupts, i.e. how long the run really was.
  cp20    - "Unknown CP load, index: 0x20" lines.
  exc     - exception entries (a fault the demo should never take).
  shot    - image size in bytes. A single flat colour compresses to about 2.3 KB at 640x480.
  report  - lines of OSReport text the demo printed.
  note    - the first OSReport line, which names the demo.

Usage: summarize.py <sweep-dir> [--json out.json]
"""

import json
import os
import re
import sys
from collections import Counter


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def image_is_flat(path):
    """Return (is_flat, dominant_rgb) for a PNG, without requiring Pillow."""
    try:
        from PIL import Image
    except ImportError:
        return None, None
    with Image.open(path) as im:
        im = im.convert("RGB")
        colors = im.getcolors(maxcolors=1 << 24)
        if not colors:
            return None, None
        colors.sort(reverse=True)
        count, rgb = colors[0]
        return count == im.width * im.height, rgb


def summarize(sweep_dir):
    rows = []
    for name in sorted(os.listdir(sweep_dir)):
        if not name.endswith(".log"):
            continue
        tag = name[:-4]
        text = read(os.path.join(sweep_dir, name))

        pe = text.count("PE_FINISHINT asserted")
        vi = text.count("VIINT asserted")
        cp20 = len(re.findall(r"Unknown CP load, index: 0x20", text))
        cp_other = sorted(set(re.findall(r"Unknown CP load, index: 0x([0-9A-F]{2})", text)) )
        exc = int((re.search(r"exception entry\s*:\s*(\d+)", text) or [None, "0"])[1] or 0)

        shot_path = os.path.join(sweep_dir, tag + ".png")
        shot = os.path.getsize(shot_path) if os.path.exists(shot_path) else 0

        report_path = os.path.join(sweep_dir, tag + ".txt")
        report_lines = read(report_path).splitlines() if os.path.exists(report_path) else []
        note = ""
        for line in report_lines:
            line = line.strip()
            if line and set(line) != {"*"} and "Dolphin OS" not in line and "Kernel built" not in line \
                    and "Console Type" not in line and "Memory " not in line and "Arena" not in line \
                    and line != "bootrom":
                note = line
                break

        rows.append(dict(tag=tag, pe=pe, vi=vi, cp20=cp20, cp_other=cp_other, exc=exc,
                         shot=shot, report=len(report_lines), note=note))
    return rows


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    sweep_dir = sys.argv[1]
    rows = summarize(sweep_dir)

    print(f"{'demo':<34}{'pe':>7}{'vi':>7}{'cp20':>6}{'exc':>6}{'shot':>8}{'rep':>5}  note")
    for r in rows:
        print(f"{r['tag']:<34}{r['pe']:>7}{r['vi']:>7}{r['cp20']:>6}{r['exc']:>6}"
              f"{r['shot']:>8}{r['report']:>5}  {r['note'][:40]}")

    flat = [r["tag"] for r in rows if r["pe"] == 0]
    print()
    print(f"{len(rows)} demos, {len(flat)} with no finished frame:")
    for t in flat:
        print("   ", t)
    print("CP addresses other than 0x20:", Counter(c for r in rows for c in r["cp_other"]))

    if "--json" in sys.argv:
        out = sys.argv[sys.argv.index("--json") + 1]
        with open(out, "w", encoding="utf-8") as f:
            json.dump(rows, f, indent=1)
        print("wrote", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
