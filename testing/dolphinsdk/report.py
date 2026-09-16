#!/usr/bin/env python3
"""Build the HTML report for a DolphinSDK demo sweep (issue #385, issue #384).

Reads the folder produced by sweep.ps1 and writes report.html next to it (plus a shots/ folder
with the screenshots the page embeds, and - when a second sweep is given - a shots_soft/ folder
with the same demos rendered by the software GFX pipeline).

The verdict is derived from the evidence (finished frames, the picture, the OSReport text) and
refined by notes.json, which holds the per-demo analysis of the interesting cases.

Usage: report.py <sweep-dir> <notes.json> <out-dir> [soft-sweep-dir]
"""

import base64
import glob
import json
import os
import re
import shutil
import sys
from collections import Counter


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def compare(a, b):
    """(mean abs difference, fraction of differing pixels) of two PNGs, or (None, None)."""
    try:
        from PIL import Image, ImageChops
        import numpy as np
    except ImportError:
        return None, None
    if not (os.path.exists(a) and os.path.exists(b)):
        return None, None
    with Image.open(a) as ia, Image.open(b) as ib:
        ia = ia.convert("RGB").resize((160, 120))
        ib = ib.convert("RGB").resize((160, 120))
        na = np.asarray(ia, dtype=np.int16)
        nb = np.asarray(ib, dtype=np.int16)
    diff = np.abs(na - nb).max(axis=2)
    return float(diff.mean()), float((diff > 32).mean())


def dominant(path):
    """(colours, top_rgb, top_percent) for a PNG, or (None, None, None)."""
    try:
        from PIL import Image
    except ImportError:
        return None, None, None
    with Image.open(path) as im:
        im = im.convert("RGB")
        cols = im.getcolors(maxcolors=1 << 24)
        if not cols:
            return None, None, None
        cols.sort(reverse=True)
        n, rgb = cols[0]
        return len(cols), rgb, n * 100 // (im.width * im.height)


def osreport_lines(path):
    if not os.path.exists(path):
        return []
    out = []
    for line in read(path).splitlines():
        line = line.replace("\ufeff", "").strip()
        if not line or set(line) <= {"*", "-", "="}:
            continue
        if any(k in line for k in ("Dolphin OS $Revision", "Kernel built", "Console Type",
                                   "Memory ", "Arena :", "bootrom", "Hardware Initialization",
                                   "Highlevel Initialization")):
            continue
        out.append(line)
    return out


def collect(sweep_dir):
    demos = []
    for log in sorted(glob.glob(os.path.join(sweep_dir, "*.log"))):
        tag = os.path.basename(log)[:-4]
        text = read(log)
        shot = os.path.join(sweep_dir, tag + ".png")
        colours, top, top_pct = dominant(shot) if os.path.exists(shot) else (None, None, None)
        rep = osreport_lines(os.path.join(sweep_dir, tag + ".txt"))
        demos.append(dict(
            tag=tag,
            name=tag.split("__", 1)[-1],
            pe=text.count("PE_FINISHINT asserted"),
            vi=text.count("VIINT asserted"),
            cp20=len(re.findall(r"Unknown CP load, index: 0x20", text)),
            exc=int((re.search(r"exception entry\s*:\s*(\d+)", text) or [None, "0"])[1] or 0),
            shot=os.path.getsize(shot) if os.path.exists(shot) else 0,
            colours=colours, top=top, top_pct=top_pct,
            cp_other=sorted({c for c in re.findall(r"Unknown CP load, index: 0x([0-9A-F]{2})", text)}),
            report=rep[:8],
            title=rep[0] if rep else "",
        ))
    return demos


def verdict(d):
    # The sweep used to count the PE finish interrupts the emulator reported per frame; the build
    # no longer prints that line, so the picture is the evidence that a frame was finished and
    # presented. A single-colour shot is a frame that did not draw (or one that never came up),
    # anything richer is a picture the demo drew.
    if d["colours"] is None:
        return "no-shot"
    if d["colours"] <= 1:
        return "flat" if d["pe"] > 0 else "no-frame"
    return "ok"


def main():
    sweep_dir, notes_path, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    soft_dir = sys.argv[4] if len(sys.argv) > 4 else None
    notes = json.load(open(notes_path, encoding="utf-8")) if os.path.exists(notes_path) else {}
    demos = collect(sweep_dir)
    soft = {d["tag"]: d for d in collect(soft_dir)} if soft_dir else {}

    for d in demos:
        d["verdict"] = verdict(d)
        d["note"] = notes.get(d["tag"], {}).get("note", "")
        d["analysis"] = notes.get(d["tag"], {}).get("analysis", "")

        # The same demo through the software GFX pipeline (issue #384)
        s = soft.get(d["tag"])
        d["soft"] = s
        d["soft_verdict"] = verdict(s) if s else None
        d["diff"] = None
        d["diff_pixels"] = None

        if s:
            diff, changed = compare(os.path.join(sweep_dir, d["tag"] + ".png"),
                                    os.path.join(soft_dir, d["tag"] + ".png"))
            d["diff"], d["diff_pixels"] = diff, changed

    shots = os.path.join(out_dir, "shots")
    os.makedirs(shots, exist_ok=True)
    for d in demos:
        src = os.path.join(sweep_dir, d["tag"] + ".png")
        if os.path.exists(src):
            shutil.copyfile(src, os.path.join(shots, d["tag"] + ".png"))

    if soft_dir:
        shots_soft = os.path.join(out_dir, "shots_soft")
        os.makedirs(shots_soft, exist_ok=True)
        for tag in soft:
            src = os.path.join(soft_dir, tag + ".png")
            if os.path.exists(src):
                shutil.copyfile(src, os.path.join(shots_soft, tag + ".png"))

    counts = Counter(d["verdict"] for d in demos)
    soft_counts = Counter(d["soft_verdict"] for d in demos if d.get("soft_verdict"))
    soft_same = len([d for d in demos if d.get("diff") is not None and d["diff"] < 6.0])
    soft_diff = len([d for d in demos if d.get("diff") is not None and d["diff"] >= 6.0])
    noted = [d for d in demos if d["tag"] in notes]
    problems = [d for d in demos if d["verdict"] != "ok" or d["tag"] in notes]
    problems.sort(key=lambda d: (d["verdict"] == "ok", d["tag"]))
    ok = [d for d in demos if d["verdict"] == "ok" and d["tag"] not in notes]
    clean = len(ok)

    def esc(s):
        return (s or "").replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    if soft_dir:
        soft_section = f"""<h2>SoftGPU: the same demos through the software GFX pipeline</h2>
<p>The right-hand column of the table is the same demo rendered by the <b>software (CPU) pipeline</b>
of issue #384 (the <code>GFX_PIPELINE = 1</code> configuration, see
<a href="../../../wiki/gfxsoft.md">wiki/gfxsoft.md</a>): the XF, the setup unit, the quad-based
rasterizers, a real TMEM, the TEV and a pixel engine with a real EFB memory array, with the copy
engine writing the XFB that the video interface scans out. No OpenGL call is involved in that path.
The runs use the same method, the same settle time and the same build as the shader column.</p>
<p class="summary"><span><b>{len(soft)}</b> captured</span>
<span class="ok"><b>{soft_counts['ok']}</b> drew a picture</span>
<span class="flat"><b>{soft_counts['flat']}</b> flat</span>
<span class="no-frame"><b>{soft_counts['no-frame']}</b> never finished a frame</span>
<span><b>{soft_same}</b> match the shader picture</span>
<span><b>{soft_diff}</b> differ</span></p>
<p><i>match/differ</i> is the mean absolute pixel difference of the two 160x120 captures: below 6
counts as a match (the demos animate, so two runs of the same demo never match exactly), above it
the two pictures are visibly different and the row is worth looking at. A demo that is already
broken under the shader backend can differ in both columns without telling anything about the
software pipeline.</p>
"""
    else:
        soft_section = ""
    soft_same = soft_same if soft_dir else 0
    soft_diff = soft_diff if soft_dir else 0

    parts = []
    parts.append(f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<title>DolphinSDK GX demo sweep (issue #385)</title>
<style>
 body {{ font-family: system-ui, sans-serif; margin: 0 auto; max-width: 1180px; padding: 24px; background:#15171c; color:#dfe3ea; }}
 h1,h2 {{ color:#fff; }} a {{ color:#7fb2ff; }}
 table {{ border-collapse: collapse; width: 100%; }}
 th, td {{ text-align: left; padding: 6px 8px; border-bottom: 1px solid #2b2f38; vertical-align: top; font-size: 14px; }}
 th {{ background:#1d2129; position: sticky; top: 0; }}
 .ok {{ color:#6ee787; }} .flat {{ color:#ffd166; }} .no-frame {{ color:#ff7b72; }}
 .shot {{ width: 320px; height: 240px; object-fit: contain; background:#000; border:1px solid #2b2f38; }}
 .shot.soft {{ border-color:#6b4f2a; }}
 .same {{ color:#6ee787; font-size:12px; }} .differ {{ color:#ffd166; font-size:12px; }}
 .kv {{ display:inline-block; min-width: 74px; color:#9aa4b2; }}
 pre {{ background:#0f1115; padding:10px; overflow:auto; font-size:12px; border-radius:6px; }}
 .card {{ background:#1a1d24; border:1px solid #2b2f38; border-radius:8px; padding:16px; margin:14px 0; }}
 .summary span {{ display:inline-block; margin-right:18px; }}
</style></head><body>
<h1>DolphinSDK GX demo sweep</h1>
<p>Every demo in <code>DolphinSDK\\HW2\\bin\\demos\\gxdemo</code> (87 ELF binaries) run through the
emulator's SDL2 port with the DolphinSDK folder mounted as a virtual DVD, for 7 seconds each.
The method is described in <code>testing/dolphinsdk/Readme.md</code>.</p>
<p><b>How to read the verdicts.</b> <i>frames</i> counts the <code>GXDrawDone</code>/PE-finish
interrupts, so a demo that never calls <code>GXDrawDone</code> (the FIFO break-point and
triple-buffer management demos, for instance) shows 0 even while it renders &mdash; the picture,
not the counter, decides. <i>flat</i> means the captured picture is a single colour. The notes give
what the demo's source says it should do, so a flat or missing picture can be judged against it.
Runs are made without controller input, so demos that only change state on a button show their
first case.</p>
<p class="summary"><span><b>{len(demos)}</b> demos</span>
<span class="ok"><b>{clean}</b> clean</span>
<span class="flat"><b>{counts['flat']}</b> drawn nothing (flat picture)</span>
<span class="no-frame"><b>{counts['no-frame']}</b> never finished a frame</span></p>
<p>Three emulator bugs found this way are fixed: the PE draw-sync token was treated as a frame boundary, which swapped the frame half-way through and lost the picture of every demo that puts <code>GXSetDrawSync</code> in the middle of a frame (<code>frb-bound-box</code> came out black); and the <code>XF_IndexLoadRegA..D</code> commands (the indexed matrix/light block loads behind <code>GXLoadPosMtxIndx</code> / <code>GXLoadTexMtxIndx</code>) were logged and thrown away, which left <code>DL-tf-mtx</code> and <code>tf-reflect</code> with no matrices at all. The report below is from the build with both fixes.</p>
<h2>What is still open</h2>
<ul>
<li><code>ind-bump-st</code>, <code>ind-bump-xyz</code> - the generated light map (TEXMAP1, an IA8
    texture the demo renders into the EFB and copies out with <code>GXCopyTex</code>) is empty. The
    geometry, the lighting arithmetic, the copy destination against the texture address, the viewport,
    the scissor and the GL draw order have all been checked and ruled out; what is left is the frame
    bookkeeping (<code>GPFrameBegin</code>/<code>GL_BeginFrame</code> and the pending copy clears)
    around a <code>PE_COPY_CMD</code> that arrives before the main loop's first frame.</li>
<li><code>tev-outline</code> - 3402 primitives and 378 display-list calls reach the command processor
    and no pixel appears; the per-vertex matrix index is the next thing to check.</li>
<li><code>G2D-test</code> - one 472-vertex quad batch per frame is drawn as a large dark shape; the 2D
    layer's scale/projection is the next thing to check.</li>
<li>The PE bounding box is not emulated: <code>PE_XBOUND</code>/<code>PE_YBOUND</code> latch the BP
    write but are never updated from the drawn geometry, and the CPU-visible
    <code>PE_PI_XBOUND0/1</code>, <code>YBOUND0/1</code> pair reads back as 0, so
    <code>GXReadBoundingBox</code> always answers <code>(0,0,0,0)</code> (gfx-pe 6.17, gfx-pe 8).</li>
</ul>
{soft_section}
<h2>Per-demo result</h2>
<table><tr><th>demo</th><th>shader backend</th><th>SoftGPU</th><th>frames</th><th>verdict</th><th>what the log and the source say</th></tr>""")

    for d in problems + ok:
        note = d["note"]
        src = d["analysis"]
        cells = f"<b>{esc(d['name'])}</b><br><span style='color:#9aa4b2'>{esc(d['title'])}</span>"
        img = f'<img class="shot" src="shots/{d["tag"]}.png">' if d["shot"] else "(none)"

        if d.get("soft"):
            simg = f'<img class="shot soft" src="shots_soft/{d["tag"]}.png">'
            if d["diff"] is None:
                badge = ""
            elif d["diff"] < 6.0:
                badge = f"<div class='same'>match (diff {d['diff']:.1f})</div>"
            else:
                badge = (f"<div class='differ'>differs ({d['diff']:.1f}; "
                         f"{d['diff_pixels']*100:.0f}% of the pixels)</div>")
            simg += badge
        else:
            simg = "(none)"

        info = (f"<span class='kv'>pe</span>{d['pe']}<br>"
                f"<span class='kv'>vi</span>{d['vi']}<br>"
                f"<span class='kv'>CP 0x20</span>{d['cp20']}")
        detail = esc(note)
        if src:
            detail += f"<pre>{esc(src)}</pre>"
        parts.append(f"<tr><td>{cells}</td><td>{img}</td><td>{simg}</td><td>{info}</td>"
                     f"<td class='{d['verdict']}'>{d['verdict']}</td><td>{detail}</td></tr>")

    parts.append("</table></body></html>")
    out = os.path.join(out_dir, "report.html")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(parts))
    print("wrote", out, "with", len(demos), "demos;", counts)
    return 0


if __name__ == "__main__":
    sys.exit(main())
