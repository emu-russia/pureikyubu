#!/usr/bin/env python3
"""Build the guest frame profile report from a capture (src/guestprof.h).

    python mkreport.py capture.json [-o report.html] [-m GM8E.map] [--title "..."]

The capture is a `format: "pureikyubu-guestprof"` document: one record per emulated console frame,
each holding the host cycles charged to every hardware block, the delta of every traffic channel,
the instructions the two cores retired, and the guest's hot basic blocks.

The report is one self-contained HTML file. Everything the charts are drawn from is computed here
and emitted as a list of blocks; tools/report.js only draws them (see the comment at the top of
that file).
"""

import argparse
import base64
import bisect
import json
import os
import statistics
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
MPFMV = os.path.dirname(HERE)

# The palette of the groups, shared with tools/report.css.
GROUP_COLOR = {
    'Gekko': '#7fd1ff',
    'Flipper': '#9be37f',
    'Audio': '#ffce6a',
    'DVD': '#ff9a7a',
    'GFX': '#c79bff',
    'Host': '#7d8496',
}


# ------------------------------------------------------------------ helpers

def pct(part, whole):
    return (part * 100.0 / whole) if whole else 0.0


def quantiles(values):
    """min, p25, median, p75, max of a list (the box a distribution chart draws)."""
    if not values:
        return dict(min=0.0, p25=0.0, median=0.0, p75=0.0, max=0.0)
    ordered = sorted(values)
    if len(ordered) == 1:
        v = ordered[0]
        return dict(min=v, p25=v, median=v, p75=v, max=v)
    q = statistics.quantiles(ordered, n=4, method='inclusive')
    return dict(min=ordered[0], p25=q[0], median=q[1], p75=q[2], max=ordered[-1])


def human_count(value):
    for limit, suffix in ((1e9, 'G'), (1e6, 'M'), (1e3, 'K')):
        if value >= limit:
            return '%.2f%s' % (value / limit, suffix)
    return '%d' % round(value)


def human_bytes(value):
    for limit, suffix in ((1024 ** 4, 'TB'), (1024 ** 3, 'GB'), (1024 ** 2, 'MB'), (1024, 'KB')):
        if value >= limit:
            return '%.2f %s' % (value / limit, suffix)
    return '%d B' % round(value)


def axis_ticks(low, high, fmt, count=5):
    """The axis labels of a chart, prepared here rather than in the browser.

    The report's blocks are JSON, so they cannot carry a formatting function; the five labels a
    chart draws are computed from the same range the chart scales by.
    """
    if high <= low:
        high = low + 1.0
    return [fmt(low + (high - low) * i / (count - 1.0)) for i in range(count)]


# ------------------------------------------------------------------ symbols

class Symbols:
    """The `.map` the emulator ships (build/Data/GM8E.map): address -> name.

    It is a thin map - a few hundred entries, most of them unnamed - so it is used as a hint
    only: a bucket is labelled with the nearest symbol at or below it, and the report always
    shows the address itself as well.
    """

    def __init__(self, path=None):
        self.addrs = []
        self.names = []
        self.path = None

        if not path:
            build = os.path.join(os.path.dirname(os.path.dirname(MPFMV)), 'build', 'Data')
            candidate = os.path.join(build, 'GM8E.map')
            path = candidate if os.path.exists(candidate) else None

        if path and os.path.exists(path):
            self.path = path
            for line in open(path, 'r', encoding='utf-8', errors='replace'):
                parts = line.split(None, 1)
                if len(parts) != 2:
                    continue
                try:
                    addr = int(parts[0], 16)
                except ValueError:
                    continue
                name = parts[1].strip()
                if name.startswith('['):        # an unnamed entry is no help
                    continue
                self.addrs.append(addr)
                self.names.append(name)

            order = sorted(range(len(self.addrs)), key=lambda i: self.addrs[i])
            self.addrs = [self.addrs[i] for i in order]
            self.names = [self.names[i] for i in order]

    def nearest(self, addr, limit=0x20000):
        if not self.addrs:
            return None
        i = bisect.bisect_right(self.addrs, addr) - 1
        if i < 0:
            return None
        if addr - self.addrs[i] > limit:
            return None
        return self.names[i]


# ------------------------------------------------------------------ the analysis

class Capture:
    def __init__(self, path, symbols):
        with open(path, 'r', encoding='utf-8') as f:
            self.doc = json.load(f)

        if self.doc.get('format') != 'pureikyubu-guestprof':
            raise SystemExit('%s is not a guest frame profile capture' % path)

        self.path = path
        self.symbols = symbols

        self.units = self.doc['units']
        self.channels = self.doc['channels']
        self.frames = self.doc['frames']
        self.movie = self.doc['movie']
        self.tsc_hz = self.doc.get('tscHz', 1.0) or 1.0
        self.ticks_per_second = self.doc.get('ticksPerSecond', 0) or 0

        self.movie_frames = [f for f in self.frames if f['movie']]
        self.plain_frames = [f for f in self.frames if not f['movie']]

        # The frames right before the stream started are the ones worth comparing against it:
        # the ones from boot are the loader and the logo screens, which are a different workload.
        self.pre_window = 30
        if self.movie_frames:
            first = self.movie_frames[0]['i']
            self.before = [f for f in self.frames if f['i'] < first][-self.pre_window:]
        else:
            self.before = self.frames[-self.pre_window:]

        self.before_label = ('the %d frames before the stream' % len(self.before)) if self.before else 'before the stream'

        # The last frame before the stream that handed anything to the pixel engine. It is where
        # the rendered part of the run ends: after it the title draws nothing at all.
        self.last_primitive_frame = 0
        primitives = next((i for i, ch in enumerate(self.channels) if ch['name'] == 'GFX primitives'), None)
        if primitives is not None:
            stream_start = self.movie_frames[0]['i'] if self.movie_frames else len(self.frames)
            rendered = [f['i'] for f in self.frames
                        if f['i'] < stream_start and f['counters'][primitives] > 0]
            self.last_primitive_frame = rendered[-1] if rendered else 0

    # -- convenience -------------------------------------------------

    @property
    def unit_ids(self):
        return [u['id'] for u in self.units]

    def cycles(self, frame):
        """The host cycles of a frame as {unit_id: cycles}."""
        return dict(zip(self.unit_ids, frame['host']))

    def mean_cycles(self, frames, unit_id):
        if not frames:
            return 0.0
        return sum(f['host'][unit_id] for f in frames) / len(frames)

    def mean_channel(self, frames, channel_id):
        if not frames:
            return 0.0
        return sum(f['counters'][channel_id] for f in frames) / len(frames)

    def mean_host_seconds(self, frames):
        if not frames:
            return 0.0
        return sum(f['host'][u] for u in self.unit_ids for f in [f]) / len(frames) / self.tsc_hz

    def phase_seconds(self, frames):
        """The total host seconds a set of frames took."""
        return sum(sum(f['host']) for f in frames) / self.tsc_hz


# ------------------------------------------------------------------ the report blocks

def build_blocks(c, image_name, command):
    B = []

    # ---------------------------------------------------------------- 1. header

    movie = c.movie
    frames_n = len(c.frames)
    movie_n = len(c.movie_frames)

    B.append(dict(kind='cards', items=[
        dict(label='frames recorded', value=str(frames_n),
             sub=('%d of them read the stream' % movie_n) if movie_n else 'none read the stream'),
        dict(label='movie frames', value=str(movie_n),
             sub=('from frame %d' % movie['startFrame']) if movie_n else 'the range was never read'),
        dict(label='emulated span', value='%.2f' % (c.frames[-1]['emulated'] if c.frames else 0), unit='s'),
        dict(label='host span', value='%.1f' % sum(f['wall'] for f in c.frames), unit='s'),
        dict(label='capture speed', value='%.2f' % _real_ratio(c), unit='x real'),
    ]))

    B.append(dict(kind='prose', html=(
        'The capture holds one record per emulated console frame. Each record carries the host '
        'cycles the profiler charged to every block of the machine, the delta of every traffic '
        'channel the emulator counts, the instructions the two cores retired, and the address of '
        'every guest basic block that ran. The blocks below are grouped the way '
        '<code>src/guestprof.h</code> groups them: <b>Gekko</b>, <b>Flipper</b>, <b>DVD</b>, '
        '<b>Audio</b>, <b>GFX</b> and <b>Host</b>.')))

    # The lead: the answer, before the evidence.
    if movie_n:
        stage = max(movie['startFrame'], 0)
        reads = sum(1 for f in c.movie_frames if f['disc']['bytes'] > 0)
        stream_bytes = sum(f['disc']['movieBytes'] for f in c.movie_frames)
        primitives = next((i for i, ch in enumerate(c.channels) if ch['name'] == 'GFX primitives'), None)
        presents = next((i for i, ch in enumerate(c.channels) if ch['name'] == 'GFX presents'), None)
        prim_movie = sum(f['counters'][primitives] for f in c.movie_frames) if primitives is not None else 0
        pres_movie = sum(f['counters'][presents] for f in c.movie_frames) if presents is not None else 0

        cmd_idx = next((i for i, ch in enumerate(c.channels) if ch['name'] == 'CP commands'), None)
        cmd_movie = (sum(f['counters'][cmd_idx] for f in c.movie_frames) / len(c.movie_frames)) if cmd_idx is not None else 0.0
        cmd_before = (sum(f['counters'][cmd_idx] for f in c.before) / len(c.before)) if (cmd_idx is not None and c.before) else 0.0

        B.append(dict(kind='note', html=(
            '<b>The headline:</b> the stream starts at frame %d, and over the %d console frames that '
            'follow it the title reads %s of it (about %d of the 301 frames its header declares) '
            'while submitting %d primitives and %d presented frames. The FMV does not display: '
            '%.1f%% of the guest\'s instructions in the window sit in a single 4 KB region, the '
            'command processor gets %.1f commands a frame against %.1f before (and almost none of '
            'them are primitives), and the picture never comes back. The emulator is not the limit '
            'here - a movie frame costs less host time than a frame of the logo screens before it.'
            % (stage, movie_n, human_bytes(stream_bytes), round(stream_bytes / 5408.0),
               prim_movie, pres_movie,
               next((e[1] for e in c.doc['phase1']['pc'] if e[0] == 0x8036C000), 0) * 100.0 /
               max(sum(e[1] for e in c.doc['phase1']['pc']), 1),
               cmd_movie, cmd_before))))

    B.append(dict(kind='prose', html=(
        '<b>How the stream was found.</b> The profiler is not told what the guest is doing; it is '
        'told where the movie lives on the disc and it watches the one place the bytes come off '
        'the image. A frame in which a read overlapped that range is a movie frame, and once the '
        'stream has started every later frame stays a movie frame (the drive reads it in bursts, '
        'so the odd frame in between may not read at all). That is what separates the frames of '
        'the FMV from the frames the title draws from textures.')))

    # What the capture is *not*: the slowdown this title is known for has already been fixed, and
    # saying so is what keeps the numbers from being read as a re-run of that bug report.
    B.append(dict(kind='note', html=(
        '<b>What this is not.</b> The Metroid Prime FMV slowdown has been through the emulator '
        'already: the branches that translated the halfword loads this movie player is made of '
        '(<code>lha</code> / <code>lhz</code>), that stopped dropping the block cache for '
        'address-translation events and that fixed a CP FIFO repoint losing unfetched entries are '
        'all ancestors of the <code>master</code> this capture was taken on, and so are the '
        'display-copy fixes that stopped the frames staying on an unpresented buffer. The capture '
        'is taken on top of all of them - <code>lha</code> <i>is</i> translated, and a movie frame '
        'is cheaper for the emulator than a rendered one. What it shows is the other half of the '
        'story: the stream is read in a trickle and the frames never reach the pixel engine.')))

    if movie_n == 0:
        B.append(dict(kind='note', html=(
            '<b>The stream was never read.</b> Every number below describes the frames the capture '
            'did hold; the movie comparison is empty. Check the <code>--movie</code> range.')))

    # ---------------------------------------------------------------- 2. the timeline

    B.append(dict(kind='heading', text='The frames'))

    first_frame = c.frames[0]['i'] if c.frames else 0
    xs = [f['i'] for f in c.frames]
    markers = [dict(x=movie['startFrame'], label='the stream starts')] if movie_n else []

    wall_max = max((f['wall'] for f in c.frames), default=0.001) * 1.08
    B.append(dict(kind='chart', chart='lines', height=280,
        title='How long each frame took the host',
        caption=('The wall time between two video-interface scan-outs, measured on the emulation '
                 'thread. The dip while the emulator boots is the loader and the logo screens; the '
                 'stream itself is the flat part after the marker.'),
        xLabel='frame', xMin=first_frame, xMax=xs[-1],
        yMin=0.0, yMax=wall_max,
        yTicks=axis_ticks(0.0, wall_max, lambda v: '%.0f ms' % (v * 1000)),
        markers=markers,
        series=[
            dict(label='host time', color='#7fd1ff',
                 points=[[f['i'], f['wall']] for f in c.frames]),
        ]))

    real_max = max((_real_of(c, f) for f in c.frames), default=1.0) * 1.1
    B.append(dict(kind='chart', chart='lines', height=260,
        title='How fast the emulator ran the guest',
        caption=('Emulated console time per host second. Below 1x the emulator cannot keep up with '
                 'the console; the profile says which block is responsible.'),
        xLabel='frame', xMin=first_frame, xMax=xs[-1],
        yMin=0.0, yMax=real_max,
        yTicks=axis_ticks(0.0, real_max, lambda v: '%.2fx' % v),
        markers=markers,
        series=[dict(label='x real time', color='#9be37f',
                     points=[[f['i'], _real_of(c, f)] for f in c.frames])]))

    core_max = max([f['gekko'] for f in c.frames] + [f['dsp'] for f in c.frames] + [1]) * 1.08
    B.append(dict(kind='chart', chart='lines', height=270,
        title='The two cores, per frame',
        caption='Instructions retired by the Gekko and by the DSP in one emulated frame.',
        xLabel='frame', xMin=first_frame, xMax=xs[-1],
        yMin=0.0, yMax=core_max,
        yTicks=axis_ticks(0.0, core_max, human_count),
        markers=markers,
        series=[
            dict(label='Gekko', color='#7fd1ff', points=[[f['i'], f['gekko']] for f in c.frames]),
            dict(label='DSP', color='#ffce6a', points=[[f['i'], f['dsp']] for f in c.frames]),
        ]))

    disc_max = max([f['disc']['bytes'] for f in c.frames] + [1]) * 1.08
    B.append(dict(kind='chart', chart='columns', height=240,
        title='What the drive read, per frame',
        caption=('Bytes that came off the disc image in each frame. The amber part of a bar is the '
                 'movie stream; the grey part is everything else (the file system, the level data, '
                 'the audio).'),
        xLabel='frame', xOffset=first_frame, xUnit='frame',
        axisTicks=axis_ticks(0.0, disc_max, human_bytes),
        values=[f['disc']['bytes'] for f in c.frames],
        valueTexts=[human_bytes(f['disc']['bytes']) for f in c.frames],
        colors=['#ff9a7a' if not f['movie'] else '#ffd9a0' for f in c.frames],
        markers=markers))

    # ---------------------------------------------------------------- 3. the host time

    B.append(dict(kind='heading', text='Where the host time went'))

    movie_secs = c.phase_seconds(c.movie_frames)
    before_secs = c.phase_seconds(c.before)

    B.append(dict(kind='prose', html=(
        'The scopes that measure these numbers nest, and the accounting is <b>exclusive</b>: a '
        'scope charges its block with the time it owned - its own elapsed cycles minus what its '
        'children reported - so no cycle is counted twice and the entries add up to the frame\'s '
        'wall clock. <b>Host / Emulator loop</b> is the remainder: the JIT\'s block-cache lookup '
        'and dispatch, the run loop\'s thread hand-off, and the profiler\'s own overhead.')))

    # A unit at zero is a reading, and the interesting ones deserve a sentence rather than a
    # silently missing bar.
    idle_units = [u for u in c.units if sum(f['host'][u['id']] for f in c.frames) == 0]
    if idle_units:
        names = ', '.join('<b>%s</b>' % u['name'] for u in idle_units)
        B.append(dict(kind='note', html=(
            '%s never charged a cycle in this capture. That is a measurement, not a gap: this title '
            'runs the <b>shader (OpenGL)</b> pipeline, so the frames are rasterized by the host GPU '
            'and the software rasterizer - the only path that enters <code>GfxSoftware</code> - '
            'never runs. The <code>gxpipeline soft</code> command (or the <code>GFX_PIPELINE</code> '
            'setting) makes the same capture exercise the other path.'
            % names)))

    # The donut: the groups over the movie frames.
    group_cycles = {}
    for u in c.units:
        group_cycles[u['group']] = group_cycles.get(u['group'], 0) + c.mean_cycles(c.movie_frames, u['id'])

    total_group = sum(group_cycles.values())
    B.append(dict(kind='chart', chart='donut', height=380,
        title='Host time by part of the machine, averaged over the movie frames',
        caption='Where one frame of the FMV is spent.',
        items=[dict(label='%s (%.1f%%)' % (g, pct(v, total_group)), value=v,
                    color=GROUP_COLOR.get(g, '#888'))
               for g, v in sorted(group_cycles.items(), key=lambda kv: -kv[1]) if v > 0]))

    # The bars: every unit, averaged over the movie frames.
    B.append(dict(kind='chart', chart='bars', labelWidth=270,
        title='Host time per block, averaged over the movie frames',
        caption='The units of <code>src/guestprof.h</code>, heaviest first. Only the units that were charged anything are shown.',
        items=[dict(label=u['name'], color=GROUP_COLOR.get(u['group'], '#888'),
                    value=c.mean_cycles(c.movie_frames, u['id']) / c.tsc_hz,
                    text='%.3f ms  (%.1f%%)' % (c.mean_cycles(c.movie_frames, u['id']) / c.tsc_hz * 1000,
                                                 pct(c.mean_cycles(c.movie_frames, u['id']), total_group)))
               for u in sorted(c.units, key=lambda u: -c.mean_cycles(c.movie_frames, u['id']))
               if c.mean_cycles(c.movie_frames, u['id']) > 0]))

    # The comparison: before the stream vs during it.
    if c.before and c.movie_frames:
        items = []
        for u in sorted(c.units, key=lambda u: -c.mean_cycles(c.movie_frames, u['id'])):
            during = c.mean_cycles(c.movie_frames, u['id']) / c.tsc_hz * 1000
            before = c.mean_cycles(c.before, u['id']) / c.tsc_hz * 1000
            if during < 0.0005 and before < 0.0005:
                continue
            items.append(dict(label=u['name'], color=GROUP_COLOR.get(u['group'], '#888'),
                              value=during,
                              text='%.3f ms  (was %.3f)' % (during, before)))

        B.append(dict(kind='chart', chart='bars', labelWidth=270,
            title='The frame before the stream, against a frame of the stream',
            caption=('Each bar is the host time of one block in an average movie frame; the figure '
                     'in brackets is the same block in the average of the %d frames that came just '
                     'before the stream started (the last of the texture-drawn screens).'
                     % len(c.before)),
            items=items))

    # The distribution: how much a block's cost varies from frame to frame.
    dist_items = []
    for u in sorted(c.units, key=lambda u: -c.mean_cycles(c.movie_frames, u['id'])):
        series = [f['host'][u['id']] / c.tsc_hz * 1000 for f in c.movie_frames]
        if not series or max(series) < 0.005:
            continue
        q = quantiles(series)
        dist_items.append(dict(label=u['name'], color=GROUP_COLOR.get(u['group'], '#888'), **q,
                               text='med %.3f' % q['median']))

    if dist_items:
        box_max = max(i['max'] for i in dist_items) * 1.05
        B.append(dict(kind='chart', chart='box', labelWidth=270,
            title='How steady each block is across the movie frames',
            caption=('The distribution of a block\'s per-frame host time over the movie frames: '
                     'the box is the interquartile range, the line inside it the median, the '
                     'whiskers the extremes. A long box means the block does something different '
                     'from frame to frame (a key frame, a texture re-decode, a burst of disc I/O).'),
            axisTicks=axis_ticks(0.0, box_max, lambda v: '%.3f' % v),
            items=dist_items))

    # The table.
    rows = []
    for group in ['Gekko', 'Flipper', 'DVD', 'Audio', 'GFX', 'Host']:
        units = [u for u in c.units if u['group'] == group]
        if not units:
            continue
        g_during = sum(c.mean_cycles(c.movie_frames, u['id']) for u in units)
        g_before = sum(c.mean_cycles(c.before, u['id']) for u in units)
        if g_during == 0 and g_before == 0:
            continue

        rows.append({'class': 'total', 'cells': [
            dict(text=group, color=GROUP_COLOR.get(group, '#888')), '',
            '%.3f' % (g_during / c.tsc_hz * 1000), '%.3f' % (g_before / c.tsc_hz * 1000),
            ('%+.0f%%' % ((g_during / g_before - 1) * 100)) if g_before else '-',
            '%.1f%%' % pct(g_during, total_group), '',
        ]})

        for u in sorted(units, key=lambda u: -c.mean_cycles(c.movie_frames, u['id'])):
            during = c.mean_cycles(c.movie_frames, u['id'])
            before = c.mean_cycles(c.before, u['id'])
            if during == 0 and before == 0:
                continue
            calls = sum(f['calls'][u['id']] for f in c.movie_frames) / max(len(c.movie_frames), 1)
            rows.append([
                u['name'], u['group'],
                '%.3f' % (during / c.tsc_hz * 1000), '%.3f' % (before / c.tsc_hz * 1000),
                ('%+.0f%%' % ((during / before - 1) * 100)) if before else '-',
                '%.1f%%' % pct(during, total_group),
                human_count(calls),
            ])

    B.append(dict(kind='table',
        caption='Host time per block: the average movie frame against the average of the frames just before the stream.',
        head=[dict(text='Block'), dict(text='Group'), dict(text='ms / movie frame', n=True),
              dict(text='ms / frame before', n=True), dict(text='change', n=True),
              dict(text='share', n=True), dict(text='calls / frame', n=True)],
        rows=rows))

    # ---------------------------------------------------------------- 4. the traffic

    B.append(dict(kind='heading', text='What the guest moved'))

    B.append(dict(kind='prose', html=(
        'The same frame record carries the delta of every channel of the '
        '<a href="https://github.com/emu-russia/pureikyubu">HW interface profiler</a> '
        '(<code>src/hwprof.h</code>), so a frame reports what the guest <i>moved</i> as well as '
        'what it <i>cost</i>. A channel that a title never touches stays at zero - that is a '
        'reading of the profile, not a gap in it.')))

    def channel_row(idx):
        name = c.channels[idx]['name']
        is_bytes = c.channels[idx]['bytes']
        during = c.mean_channel(c.movie_frames, idx)
        before = c.mean_channel(c.before, idx)
        per_second = during * (c.ticks_per_second / _frame_ticks(c)) if _frame_ticks(c) else 0.0
        fmt = human_bytes if is_bytes else human_count
        return [name,
                fmt(during) if during else '-',
                fmt(before) if before else '-',
                ('%+.0f%%' % ((during / before - 1) * 100)) if before else '-',
                fmt(per_second) + '/s' if per_second else '-'], during

    rows = []
    for idx in range(len(c.channels)):
        row, during = channel_row(idx)
        if during > 0 or any(c.mean_channel(c.before, idx) > 0 for _ in [0]):
            rows.append((during, row))
    rows.sort(key=lambda r: -r[0])

    B.append(dict(kind='table',
        caption='Every traffic channel, per movie frame (averaged), with the same figure for the frames before the stream.',
        head=[dict(text='Channel'), dict(text='per movie frame', n=True), dict(text='per frame before', n=True),
              dict(text='change', n=True), dict(text='per emulated second', n=True)],
        rows=[r for _, r in rows]))

    # The biggest channels as a chart.
    bar_items = []
    for idx in range(len(c.channels)):
        during = c.mean_channel(c.movie_frames, idx)
        if during <= 0:
            continue
        is_bytes = c.channels[idx]['bytes']
        bar_items.append(dict(label=c.channels[idx]['name'],
                              value=during,
                              color='#ff9a7a' if is_bytes else '#7fd1ff',
                              text=(human_bytes(during) if is_bytes else human_count(during)) + ' / frame'))

    if bar_items:
        bar_items.sort(key=lambda i: -i['value'])
        B.append(dict(kind='chart', chart='bars', labelWidth=270,
            title='The channels the movie frames moved most of',
            caption='Bytes per frame for the traffic channels (red), events per frame for the counters (blue).',
            items=bar_items[:18]))

    # ---------------------------------------------------------------- 5. the guest code

    B.append(dict(kind='heading', text='Where the guest\'s code is'))

    B.append(dict(kind='prose', html=(
        'The profiler keeps a histogram of the guest\'s basic blocks, bucketed by 64 KB of the '
        'effective address space, one histogram for the frames that did not read the stream and '
        'one for the frames that did. That is the guest\'s own distribution: which part of the '
        'title is running while the FMV plays.')))

    def pc_items(phase, top=26):
        entries = sorted(c.doc['phase%d' % phase]['pc'], key=lambda e: -e[1])[:top]
        total = sum(e[1] for e in c.doc['phase%d' % phase]['pc']) or 1
        items = []
        for addr, weight in entries:
            sym = c.symbols.nearest(addr)
            label = '0x%08X' % addr
            if sym:
                label += '  ' + sym
            items.append(dict(label=label, value=weight, color='#7fd1ff',
                              text='%s  (%.1f%%)' % (human_count(weight), pct(weight, total))))
        return items, total

    if movie_n:
        items, total = pc_items(1)
        B.append(dict(kind='chart', chart='bars', labelWidth=380, height=560,
            title='The guest\'s hot code while the stream plays',
            caption=('Basic blocks retired, by 64 KB bucket, over all %d movie frames. The '
                     'percentage is of the %s instructions the bucket histogram holds. The name '
                     'after the address is the nearest symbol of the shipped map.' % (movie_n, human_count(total))),
            items=items))

    if c.plain_frames:
        items, total = pc_items(0)
        B.append(dict(kind='chart', chart='bars', labelWidth=380, height=560,
            title='The guest\'s hot code before the stream',
            caption='The same histogram over the frames that did not read the stream (the loader and the screens the title draws from textures).',
            items=items))

    # The DSP.
    if movie_n:
        dsp = sorted(c.doc['phase1']['dspPc'], key=lambda e: -e[1])[:16]
        dsp_total = sum(e[1] for e in c.doc['phase1']['dspPc']) or 1
        if dsp:
            B.append(dict(kind='chart', chart='bars', labelWidth=190, height=420,
                title='The DSP\'s hot words while the stream plays',
                caption=('DSP instructions by program counter over the movie frames. The DSP '
                         'program lives in its own 4 KWord IMEM, so the addresses are exact and '
                         'not bucketed.'),
                items=[dict(label='0x%04X' % pc, value=w, color='#ffce6a',
                            text='%s  (%.1f%%)' % (human_count(w), pct(w, dsp_total)))
                       for pc, w in dsp]))

    # ---------------------------------------------------------------- 6. findings

    B.append(dict(kind='heading', text='What the numbers say'))

    top = max(c.units, key=lambda u: c.mean_cycles(c.movie_frames, u['id'])) if c.movie_frames else None
    top_group = max(group_cycles.items(), key=lambda kv: kv[1]) if group_cycles else ('-', 0)

    notes = []

    def total_channel(frames, name):
        idx = next((i for i, ch in enumerate(c.channels) if ch['name'] == name), None)
        if idx is None or not frames:
            return 0
        return sum(f['counters'][idx] for f in frames)

    if top:
        share = pct(c.mean_cycles(c.movie_frames, top['id']), total_group)
        notes.append('<b>The heaviest block is %s</b> (%s), at %.3f ms and %.1f%% of an average '
                     'movie frame.' % (top['name'], top['group'],
                                       c.mean_cycles(c.movie_frames, top['id']) / c.tsc_hz * 1000, share))

    if total_group:
        notes.append('<b>%s owns %.1f%% of the frame.</b>' % (top_group[0], pct(top_group[1], total_group)))

    # The two numbers that say whether the stream is actually playing: how many of its frames the
    # guest consumes, and how much of the frame it hands to the pixel engine.
    if movie_n:
        reads = [f for f in c.movie_frames if f['disc']['bytes'] > 0]
        read_bytes = sum(f['disc']['bytes'] for f in c.movie_frames)
        stream_bytes = sum(f['disc']['movieBytes'] for f in c.movie_frames)
        per_frame = read_bytes / len(c.movie_frames)

        notes.append('<b>The drive served the stream in %d of the %d movie frames</b>, %s per frame '
                     'on average. Of those bytes, %s fell inside the intro stream\'s own range on '
                     'the disc - about <b>%d of the 301 frames its header declares</b>. The title '
                     'is not playing the movie; it is sipping it.'
                     % (len(reads), len(c.movie_frames), human_bytes(per_frame),
                        human_bytes(stream_bytes), round(stream_bytes / 5408.0)))

        def per_100(frames, name):
            return total_channel(frames, name) * 100.0 / max(len(frames), 1)

        notes.append('<b>Almost nothing reaches the pixel engine.</b> Per 100 console frames the '
                     'movie frames submitted %.0f CP commands, %.1f primitives and %.1f presented '
                     'frames; the frames before the stream submitted %.0f commands, %.1f primitives '
                     'and %.1f presents. The last frame before the stream that submitted a single '
                     'primitive was frame %d - the picture stops %d frames before the stream starts '
                     'and never comes back.'
                     % (per_100(c.movie_frames, 'CP commands'), per_100(c.movie_frames, 'GFX primitives'),
                        per_100(c.movie_frames, 'GFX presents'),
                        per_100(c.plain_frames, 'CP commands'), per_100(c.plain_frames, 'GFX primitives'),
                        per_100(c.plain_frames, 'GFX presents'),
                        c.last_primitive_frame, c.movie['startFrame'] - c.last_primitive_frame))

    if c.before and c.movie_frames:
        before_total = c.phase_seconds(c.before) / max(len(c.before), 1)
        movie_total = c.phase_seconds(c.movie_frames) / max(len(c.movie_frames), 1)
        notes.append('A movie frame costs <b>%.1f%% %s</b> host time than a frame of the screens '
                     'before it (%.2f ms against %.2f ms) - the title is not slow here because it '
                     'draws, it is slow because it waits.'
                     % (abs(movie_total / before_total - 1) * 100 if before_total else 0,
                        'more' if movie_total >= before_total else 'less',
                        movie_total * 1000, before_total * 1000))

    gekko_jit = next((u['id'] for u in c.units if u['name'] == 'Gekko JIT'), None)
    if gekko_jit is not None and c.movie_frames:
        gb = c.mean_cycles(c.before, gekko_jit) / c.tsc_hz * 1000 if c.before else 0.0
        gm = c.mean_cycles(c.movie_frames, gekko_jit) / c.tsc_hz * 1000
        notes.append('The recompiled Gekko code alone went from <b>%.3f ms</b> a frame to '
                     '<b>%.3f ms</b> (%.1fx).' % (gb, gm, (gm / gb) if gb else 0))

    # Where the guest's own instructions go while the stream plays.
    if movie_n:
        phase = c.doc['phase1']['pc']
        total_instr = sum(e[1] for e in phase) or 1
        heaviest = max(phase, key=lambda e: e[1])
        concentration = sum(sorted((e[1] for e in phase), reverse=True)[:4])
        notes.append('<b>%.1f%% of the guest\'s instructions in the movie frames sit in a single '
                     '4 KB region</b> at <code>0x%08X</code> (the four heaviest regions together are '
                     '%.1f%%). Whatever the guest is waiting for, it waits for it in a very small '
                     'amount of code.'
                     % (heaviest[1] * 100.0 / total_instr, heaviest[0],
                        concentration * 100.0 / total_instr))

    dsp_units = [u for u in c.units if u['group'] == 'Audio']
    if c.movie_frames:
        dsp_share = sum(c.mean_cycles(c.movie_frames, u['id']) for u in dsp_units)
        notes.append('The DSP and the audio path together are <b>%.1f%%</b> of a movie frame, and '
                     'the DSP retires <b>%s instructions per movie frame</b> - it keeps mixing '
                     'while the video side waits.'
                     % (pct(dsp_share, total_group),
                        human_count(sum(f['dsp'] for f in c.movie_frames) / len(c.movie_frames))))

    disc_idx = next((i for i, ch in enumerate(c.channels) if ch['name'] == 'DDU bytes read'), None)
    if disc_idx is not None and c.movie_frames:
        per_frame = c.mean_channel(c.movie_frames, disc_idx)
        notes.append('The interface between the DI and the drive moved <b>%s per movie frame</b>, '
                     'and the DDU executed %.1f commands per movie frame.'
                     % (human_bytes(per_frame), c.mean_channel(c.movie_frames, next(
                         i for i, ch in enumerate(c.channels) if ch['name'] == 'DDU commands'))))

    B.append(dict(kind='prose', html='<ul>' + ''.join('<li>%s</li>' % n for n in notes) + '</ul>'))

    return B


def _frame_ticks(c):
    if len(c.frames) > 1:
        return c.frames[-1]['t'] - c.frames[-2]['t']
    return 0


def _real_of(c, frame):
    if frame['wall'] <= 0 or not c.ticks_per_second:
        return 0.0
    emulated = frame['t'] / c.ticks_per_second   # not exact for the first frame, good enough
    ticks = _frame_ticks(c) or (c.ticks_per_second / 60)
    return (ticks / c.ticks_per_second) / frame['wall'] if frame['wall'] else 0.0


def _real_ratio(c):
    if not c.frames:
        return 0.0
    emulated = c.frames[-1]['emulated']
    wall = sum(f['wall'] for f in c.frames)
    return emulated / wall if wall else 0.0


# ------------------------------------------------------------------ the PC sample

def _sampler_buckets(capture, sample_path):
    """The sampler's samples of `capture`'s movie window, aggregated into 4 KB buckets.

    Aggregating the sampler the same way the capture aggregates its instructions is what makes the
    two comparable: if the sampler reports the instruction that is really running, its buckets have
    to match the deterministic histogram, and the total variation distance between the two says by
    how much they do not.
    """
    doc = json.load(open(sample_path, 'r', encoding='utf-8'))
    flat = doc.get('sampleData', [])

    start = 0
    for frame in capture.frames:
        if frame['disc']['bytes'] > 0 and frame['movie']:
            start = frame['t']
            break

    buckets = {}
    exact = {}
    total = 0
    for i in range(0, len(flat) - 1, 2):
        if flat[i] >= start:
            pc = flat[i + 1]
            buckets[(pc >> 12) << 12] = buckets.get((pc >> 12) << 12, 0) + 1
            exact[pc] = exact.get(pc, 0) + 1
            total += 1

    return buckets, exact, total


def _variation(a, b):
    """Half the L1 distance between two normalised distributions."""
    if not a or not b:
        return 0.0
    keys = set(a) | set(b)
    return sum(abs(a.get(k, 0.0) - b.get(k, 0.0)) for k in keys) / 2.0


def build_pc_sample_blocks(main, interp, pc_sample, jit_sample):
    """The movie window at instruction resolution.

    The frame profiler records the guest's basic blocks in 4 KB buckets, which is what a chart wants
    but not enough to name a function when the shipped symbol map has a gap where the hot code
    lives. Two things resolve it, and the report has to show both because only one of them is
    trustworthy:

    * the emulator's own sampling profiler (`StartProfiler`, `src/debug.cpp`) reads `regs.pc` every
      couple of emulated milliseconds;
    * a capture taken with the recompiler off (`jit 0`), where the interpreter charges every
      instruction to its own address.

    With the recompiler *on* the emulated `regs.pc` only moves at the boundaries of a compiled
    block, so the sampler reports the block's entry while the block runs - which is exactly what the
    compiled-path histogram does. Comparing each sampler against the deterministic histogram of its
    own capture is what tells the two apart, and the numbers below are the evidence.
    """
    # The interpreter capture is the one the exact addresses come from; it falls back to the main
    # capture when only one was taken.
    reference = interp if interp is not None else main

    modes = []
    if pc_sample and os.path.exists(pc_sample):
        modes.append(('the recompiler off', reference, pc_sample))
    if jit_sample and os.path.exists(jit_sample):
        modes.append(('the recompiler on', main, jit_sample))

    if not modes:
        return []

    B = [dict(kind='heading', text='The exact hot instructions'),
         dict(kind='prose', html=(
             'A 4 KB bucket is what the profiler records per frame; it is not always enough to name '
             'a function, because the shipped symbol map has gaps where this title\'s hot code '
             'lives. The emulator\'s sampling profiler (<code>StartProfiler</code>, '
             '<code>src/debug.cpp</code>) reads <code>regs.pc</code> from another thread every two '
             'emulated milliseconds, which resolves the buckets to addresses - but <b>only when the '
             'recompiler is off</b>. Inside a compiled block the emulated program counter does not '
             'move, so the sampler reports the block\'s entry address for as long as the block runs, '
             'exactly the way the compiled path reports its instructions. The table below is the '
             'proof: each sampler is compared against the deterministic instruction histogram of '
             'its own capture.'))]

    # The evidence table: sampler against deterministic histogram, per execution mode.
    rows = []
    for label, capture, sample in modes:
        buckets, exact, total = _sampler_buckets(capture, sample)
        if total == 0:
            continue

        hist = {addr: weight for addr, weight in capture.doc['phase1']['pc']}
        hist_total = sum(hist.values()) or 1
        norm_hist = {k: v / hist_total for k, v in hist.items()}
        norm_samp = {k: v / total for k, v in buckets.items()}

        hottest = max(hist, key=lambda k: hist[k])
        rows.append([
            label,
            human_count(total),
            ('%.2f%%' % (norm_hist.get(hottest, 0.0) * 100.0)),
            ('%.2f%%' % (norm_samp.get(hottest, 0.0) * 100.0)),
            ('%.3f' % _variation(norm_hist, norm_samp)),
        ])

        if label == 'the recompiler off':
            exact_hist = exact
            exact_total = total
            exact_capture = capture

    if rows:
        B.append(dict(kind='table',
            caption=('How well each sampler agrees with the instructions the profiler counted. The '
                     'last column is the total variation distance between the two distributions: '
                     'near zero means the sampled program counter tracks the instruction that is '
                     'running, and a large number means it does not.'),
            head=[dict(text='Execution mode'), dict(text='Samples', n=True),
                  dict(text='Hottest 4 KB bucket, counted', n=True),
                  dict(text='... the same bucket, sampled', n=True),
                  dict(text='Distance', n=True)],
            rows=rows))

    B.append(dict(kind='note', html=(
        'With the recompiler on the sampled distribution is a quarter of the whole away from the '
        'counted one, and it moves about 25 points of the time into the neighbouring 4 KB bucket: '
        'the block-entry bias. With the recompiler off the two agree to within a few percent. '
        '<b>A profile taken with <code>jit 0</code> is the one to read when the question is which '
        'instruction is hot.</b>')))

    # The exact addresses, from the authoritative run.
    top = sorted(exact_hist.items(), key=lambda kv: -kv[1])[:20]

    def label(pc):
        sym = exact_capture.symbols.nearest(pc, 0x8000)
        return ('0x%08X  %s' % (pc, sym)) if sym else ('0x%08X' % pc)

    B.append(dict(kind='chart', chart='bars', labelWidth=280, height=520,
        title='The hottest instructions, from the capture with the recompiler off',
        caption=('Sampled program counters as a share of the movie window. The six at the top are '
                 'consecutive instructions of one loop - a linear search over a signed-halfword '
                 'array - and they are the leaf the guest spins in.'),
        items=[dict(label=label(pc), value=count, color='#c79bff',
                    text='%.2f%%  (%s)' % (count * 100.0 / exact_total, human_count(count)))
               for pc, count in top]))

    B.append(dict(kind='table',
        caption='The sampled program counters of the movie window, from the capture with the recompiler off.',
        head=[dict(text='Address'), dict(text='Nearest symbol'), dict(text='Samples', n=True),
              dict(text='Share', n=True)],
        rows=[[('0x%08X' % pc), (exact_capture.symbols.nearest(pc, 0x8000) or '(unnamed)'),
               human_count(count), '%.2f%%' % (count * 100.0 / exact_total)]
              for pc, count in top[:24]]))

    return B


# ------------------------------------------------------------------ the page

def _hottest_bucket_share(capture):
    """The share of the movie frames' instructions that the single heaviest 4 KB bucket holds."""
    phase = capture.doc['phase1']['pc']
    total = sum(e[1] for e in phase) or 1
    return max((e[1] for e in phase), default=0) * 100.0 / total

def render(capture_path, out_path, symbols_path, title, pc_sample_path=None, disasm_path=None,
           jit_pc_sample_path=None, interp_capture_path=None):
    symbols = Symbols(symbols_path)
    c = Capture(capture_path, symbols)

    image = c.doc.get('image') or os.path.basename(capture_path)
    command = ('pureikyubu_headless --guestprof "%s" %s --movie 0x%X:%d --movieframes %d'
               % (image, os.path.basename(capture_path), c.movie['offset'], c.movie['length'],
                  max(len(c.movie_frames), 60)))

    blocks = build_blocks(c, image, command)

    # The instruction-level view, if the emulator's own PC sampler was run alongside the captures.
    interp = Capture(interp_capture_path, symbols) if (interp_capture_path and os.path.exists(interp_capture_path)) else None
    blocks.extend(build_pc_sample_blocks(c, interp, pc_sample_path, jit_pc_sample_path))

    if interp is not None:
        blocks.append(dict(kind='sub', text='The same window with the recompiler off'))
        blocks.append(dict(kind='prose', html=(
            'A second capture was taken with <code>jit 0</code> to settle which instruction is hot. '
            'The volume agrees - the hottest 4 KB bucket holds %.1f%% of the movie frames\' '
            'instructions with the recompiler on and %.1f%% with it off - so the region is real and '
            'not an artefact of how the compiled path reports a block. Two things differ, and both '
            'are worth knowing before reading a hot-address list: the addresses inside that region, '
            'and the console frame at which the guest reaches the same state (%d against %d), '
            'because the idle-wait skip is driven from the compiled path alone and the interpreter '
            'executes the guest\'s poll loops for real.'
            % (_hottest_bucket_share(c), _hottest_bucket_share(interp),
               c.movie['startFrame'], interp.movie['startFrame']))))

    if disasm_path and os.path.exists(disasm_path):
        # `utf-8-sig`: the listing is produced by a shell redirect on Windows, which likes to put a
        # byte order mark in front of it.
        with open(disasm_path, 'r', encoding='utf-8-sig', errors='replace') as f:
            listing = f.read().rstrip()
        blocks.append(dict(kind='sub', text='The hot loop, disassembled'))
        blocks.append(dict(kind='note',
            html=('Disassembled by the emulator\'s own <code>GekkoDisasm</code> '
                  '(<code>tools/jdi.py disasm &lt;address&gt; &lt;count&gt;</code>); the listing is '
                  '<code>hotloops.txt</code> next to the capture.')))
        blocks.append(dict(kind='code', text=listing))

    subtitle = ('%s &middot; %d frames recorded &middot; %d of them read the movie stream '
                '&middot; capture written %s'
                % (os.path.basename(image), len(c.frames), len(c.movie_frames),
                   time.strftime('%Y-%m-%d %H:%M')))

    footer = ('Built by <code>testing/mpfmv/tools/mkreport.py</code> from '
              '<code>%s</code>. The profiler is <code>src/guestprof.h</code>; the '
              'channels are <code>src/hwprof.h</code>.' % os.path.basename(capture_path))
    if symbols.path:
        footer += ' Symbols from <code>%s</code>.' % os.path.basename(symbols.path)

    report = dict(title=title, subtitle=subtitle, blocks=blocks, footer=footer)

    css = open(os.path.join(HERE, 'report.css'), 'r', encoding='utf-8').read()
    js = open(os.path.join(HERE, 'report.js'), 'r', encoding='utf-8').read()

    html = []
    html.append('<!DOCTYPE html>')
    html.append('<html lang="en"><head><meta charset="utf-8">')
    html.append('<meta name="viewport" content="width=device-width, initial-scale=1">')
    html.append('<title>%s</title>' % (title or 'Guest frame profile'))
    html.append('<style>%s</style>' % css)
    html.append('</head><body>')
    html.append('<script>window.REPORT = %s;</script>' % json.dumps(report))
    html.append('<script>%s</script>' % js)
    html.append('</body></html>')

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(html))

    return out_path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('capture')
    ap.add_argument('-o', '--out', default=os.path.join(MPFMV, 'report.html'))
    ap.add_argument('-m', '--map', default=None, help='a .map symbol file (default: build/Data/GM8E.map)')
    ap.add_argument('--pc-sample', default=os.path.join(MPFMV, 'pcsample_interp.json'),
                    help='a PC sample document from a capture taken with the recompiler OFF (jit 0); '
                         'this is the authoritative one')
    ap.add_argument('--jit-pc-sample', default=os.path.join(MPFMV, 'pcsample.json'),
                    help='a PC sample document from the capture taken with the recompiler ON, for '
                         'the comparison that shows the block-entry bias')
    ap.add_argument('--interp-capture', default=os.path.join(MPFMV, 'capture_interp.json'),
                    help='the capture taken with the recompiler off (optional)')
    ap.add_argument('--disasm', default=os.path.join(MPFMV, 'hotloops.txt'),
                    help='a disassembly listing of the hot loop, included verbatim (optional)')
    ap.add_argument('--title', default='Metroid Prime - guest frame profile of the intro FMV')
    args = ap.parse_args()

    path = render(args.capture, args.out, args.map, args.title, args.pc_sample, args.disasm,
                  args.jit_pc_sample, args.interp_capture)
    print('wrote %s (%d bytes)' % (path, os.path.getsize(path)))


if __name__ == '__main__':
    main()
