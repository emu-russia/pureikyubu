/* The guest frame profile report: a block renderer.
 *
 * The analysis lives in mkreport.py, which emits `window.REPORT` as a list of blocks with a
 * `kind`. This file only draws them. Keeping the split there means a new question is answered by
 * adding a block on the Python side and, at most, a `kind` here - and it keeps the JavaScript
 * small enough to read in one sitting.
 *
 * Everything is inline SVG, so the report is one file that works offline.
 */
'use strict';

const NS = 'http://www.w3.org/2000/svg';

function svgEl(tag, attrs, text) {
  const node = document.createElementNS(NS, tag);
  if (attrs) for (const k in attrs) if (attrs[k] !== null && attrs[k] !== undefined) node.setAttribute(k, attrs[k]);
  if (text !== undefined && text !== null) node.textContent = text;
  return node;
}

function el(tag, attrs, text) {
  const node = document.createElement(tag);
  if (attrs) for (const k in attrs) {
    if (k === 'class') node.className = attrs[k];
    else if (k === 'html') node.innerHTML = attrs[k];
    else node.setAttribute(k, attrs[k]);
  }
  if (text !== undefined && text !== null) node.textContent = text;
  return node;
}

/* ------------------------------------------------------------------ numbers */

function fmt(value, digits) {
  if (value === null || value === undefined || !isFinite(value)) return '-';
  const d = (digits === undefined) ? 2 : digits;
  return value.toLocaleString('en-US', { minimumFractionDigits: d, maximumFractionDigits: d });
}

function fmtInt(value) {
  if (value === null || value === undefined) return '-';
  return Math.round(value).toLocaleString('en-US');
}

/* A human duration from seconds. */
function fmtTime(seconds) {
  if (seconds < 1e-3) return fmt(seconds * 1e6, 0) + ' us';
  if (seconds < 1) return fmt(seconds * 1e3, 1) + ' ms';
  if (seconds < 60) return fmt(seconds, 2) + ' s';
  const m = Math.floor(seconds / 60);
  return m + ' m ' + fmt(seconds - m * 60, 1) + ' s';
}

/* A byte count with 1024-based units, the way the emulator prints traffic. */
function fmtBytes(value) {
  const suffix = ['B', 'KB', 'MB', 'GB', 'TB'];
  let scaled = value, unit = 0;
  while (scaled >= 1024 && unit < 4) { scaled /= 1024; unit++; }
  return fmt(scaled, unit === 0 ? 0 : 2) + ' ' + suffix[unit];
}

/* ------------------------------------------------------------------ charts */

const CHART_W = 1000;

/* A horizontal bar chart: one row per item, the label to the left of the bar. */
function chartBars(block) {
  const rowH = 22, gap = 6, top = 6, left = block.labelWidth || 250, right = 90;
  const items = block.items.filter(i => i.value > 0 || block.showZero);
  const h = top + items.length * (rowH + gap) + 6;
  const plotW = CHART_W - left - right;
  const max = Math.max(...items.map(i => i.value), 1e-12);

  const root = svgEl('svg', { viewBox: `0 0 ${CHART_W} ${h}`, role: 'img' });

  items.forEach((item, n) => {
    const y = top + n * (rowH + gap);
    const w = Math.max(1, (item.value / max) * plotW);
    const color = item.color || 'var(--accent)';

    const label = svgEl('text', {
      x: left - 10, y: y + rowH * 0.72, 'text-anchor': 'end',
      fill: 'var(--ink)', 'font-size': 12.5
    }, item.label);
    root.appendChild(label);

    root.appendChild(svgEl('rect', {
      x: left, y: y, width: plotW, height: rowH, rx: 3, fill: '#0e1015'
    }));

    const bar = svgEl('rect', {
      x: left, y: y, width: w, height: rowH, rx: 3, fill: color
    });
    bar.appendChild(svgEl('title', null, `${item.label}: ${item.text || fmt(item.value)}`));
    root.appendChild(bar);

    root.appendChild(svgEl('text', {
      x: left + w + 8, y: y + rowH * 0.72, fill: 'var(--ink)', 'font-size': 12,
      'font-family': 'Consolas, monospace'
    }, item.text || fmt(item.value)));
  });

  return root;
}

/* The axis label of tick `g` of `count`, from the precomputed list or a plain number. */
function axisLabel(block, g, value, digits) {
  if (block.axisTicks) return block.axisTicks[g];
  if (block.yTicks) return block.yTicks[g];
  return fmt(value, digits === undefined ? 1 : digits);
}

/* A vertical bar chart: the x axis is an index (a frame, a bucket), the bars are values. */
function chartColumns(block) {
  const left = 62, right = 14, top = 12, bottom = 34;
  const h = block.height || 240;
  const plotW = CHART_W - left - right, plotH = h - top - bottom;
  const values = block.values;
  const max = block.max || Math.max(...values, 1e-12);
  const bw = plotW / values.length;

  const root = svgEl('svg', { viewBox: `0 0 ${CHART_W} ${h}`, role: 'img' });

  for (let g = 0; g <= 4; g++) {
    const y = top + plotH - (g / 4) * plotH;
    root.appendChild(svgEl('line', { x1: left, y1: y, x2: left + plotW, y2: y, stroke: 'var(--line)', 'stroke-width': 1 }));
    root.appendChild(svgEl('text', {
      x: left - 8, y: y + 4, 'text-anchor': 'end', fill: 'var(--dim)', 'font-size': 11,
      'font-family': 'Consolas, monospace'
    }, axisLabel(block, g, (g / 4) * max)));
  }

  values.forEach((v, i) => {
    const bh = Math.max(v > 0 ? 1 : 0, (v / max) * plotH);
    const bar = svgEl('rect', {
      x: left + i * bw, y: top + plotH - bh, width: Math.max(1, bw - 1), height: bh,
      fill: block.colors ? block.colors[i] : (block.color || 'var(--accent)')
    });
    const text = block.valueTexts ? block.valueTexts[i] : fmt(v);
    bar.appendChild(svgEl('title', null, `${block.xLabel || 'x'}=${block.xOffset + i}: ${text}`));
    root.appendChild(bar);
  });

  if (block.markers) block.markers.forEach(m => {
    const x = left + (m.x - block.xOffset) * bw;
    root.appendChild(svgEl('line', { x1: x, y1: top, x2: x, y2: top + plotH, stroke: '#ffb454', 'stroke-width': 1.5, 'stroke-dasharray': '4 3' }));
    root.appendChild(svgEl('text', { x: x + 5, y: top + 12, fill: '#ffb454', 'font-size': 11.5 }, m.label));
  });

  root.appendChild(svgEl('text', { x: left, y: h - 10, fill: 'var(--dim)', 'font-size': 11.5 }, block.xLabel || ''));
  root.appendChild(svgEl('text', { x: left + plotW, y: h - 10, 'text-anchor': 'end', fill: 'var(--dim)', 'font-size': 11.5 },
    (block.xUnit || '') + ' ' + fmtInt(block.xOffset + values.length - 1)));
  return root;
}

/* A line chart: several series over a common x. */
function chartLines(block) {
  const left = 62, right = 118, top = 12, bottom = 34;
  const h = block.height || 260;
  const plotW = CHART_W - left - right, plotH = h - top - bottom;

  const all = [];
  block.series.forEach(s => s.points.forEach(p => all.push(p[1])));

  const xMin = block.xMin !== undefined ? block.xMin : Math.min(...block.series.flatMap(s => s.points.map(p => p[0])));
  const xMax = block.xMax !== undefined ? block.xMax : Math.max(...block.series.flatMap(s => s.points.map(p => p[0])), xMin + 1);
  const yMax = block.yMax !== undefined ? block.yMax : Math.max(...all, 1e-12);
  const yMin = block.yMin !== undefined ? block.yMin : 0;

  const X = x => left + ((x - xMin) / (xMax - xMin || 1)) * plotW;
  const Y = y => top + plotH - ((y - yMin) / (yMax - yMin || 1)) * plotH;

  const root = svgEl('svg', { viewBox: `0 0 ${CHART_W} ${h}`, role: 'img' });

  for (let g = 0; g <= 4; g++) {
    const y = top + plotH - (g / 4) * plotH;
    root.appendChild(svgEl('line', { x1: left, y1: y, x2: left + plotW, y2: y, stroke: 'var(--line)' }));
    root.appendChild(svgEl('text', {
      x: left - 8, y: y + 4, 'text-anchor': 'end', fill: 'var(--dim)', 'font-size': 11,
      'font-family': 'Consolas, monospace'
    }, axisLabel(block, g, yMin + (g / 4) * (yMax - yMin))));
  }

  if (block.markers) block.markers.forEach(m => {
    root.appendChild(svgEl('line', { x1: X(m.x), y1: top, x2: X(m.x), y2: top + plotH, stroke: '#ffb454', 'stroke-width': 1.5, 'stroke-dasharray': '4 3' }));
    root.appendChild(svgEl('text', { x: X(m.x) + 5, y: top + 12, fill: '#ffb454', 'font-size': 11.5 }, m.label));
  });

  block.series.forEach((s, n) => {
    const d = s.points.map((p, i) => (i ? 'L' : 'M') + X(p[0]).toFixed(1) + ' ' + Y(p[1]).toFixed(1)).join(' ');
    const path = svgEl('path', {
      d, fill: 'none', stroke: s.color || 'var(--accent)',
      'stroke-width': s.width || 1.6, 'stroke-dasharray': s.dashed ? '5 4' : null,
      'stroke-linejoin': 'round'
    });
    path.appendChild(svgEl('title', null, s.label));
    root.appendChild(path);

    const ly = top + 14 + n * 17;
    root.appendChild(svgEl('line', { x1: left + plotW + 12, y1: ly - 4, x2: left + plotW + 30, y2: ly - 4, stroke: s.color || 'var(--accent)', 'stroke-width': 2.4 }));
    root.appendChild(svgEl('text', { x: left + plotW + 36, y: ly, fill: 'var(--dim)', 'font-size': 11.5 }, s.label));
  });

  root.appendChild(svgEl('text', { x: left, y: h - 10, fill: 'var(--dim)', 'font-size': 11.5 }, block.xLabel || ''));
  root.appendChild(svgEl('text', { x: left + plotW, y: h - 10, 'text-anchor': 'end', fill: 'var(--dim)', 'font-size': 11.5 }, fmtInt(xMax)));
  return root;
}

/* A stacked column chart: one column per x, one segment per category. */
function chartStacked(block) {
  const left = 62, right = 118, top = 12, bottom = 34;
  const h = block.height || 260;
  const plotW = CHART_W - left - right, plotH = h - top - bottom;

  const xs = block.x;
  const totals = xs.map((_, i) => block.categories.reduce((a, c) => a + (c.values[i] || 0), 0));
  const max = Math.max(...totals, 1e-12);
  const bw = plotW / xs.length;

  const root = svgEl('svg', { viewBox: `0 0 ${CHART_W} ${h}`, role: 'img' });

  for (let g = 0; g <= 4; g++) {
    const y = top + plotH - (g / 4) * plotH;
    root.appendChild(svgEl('line', { x1: left, y1: y, x2: left + plotW, y2: y, stroke: 'var(--line)' }));
    root.appendChild(svgEl('text', {
      x: left - 8, y: y + 4, 'text-anchor': 'end', fill: 'var(--dim)', 'font-size': 11,
      'font-family': 'Consolas, monospace'
    }, axisLabel(block, g, (g / 4) * max)));
  }

  if (block.markers) block.markers.forEach(m => {
    const x = left + (m.x - xs[0]) * bw;
    root.appendChild(svgEl('line', { x1: x, y1: top, x2: x, y2: top + plotH, stroke: '#ffb454', 'stroke-width': 1.5, 'stroke-dasharray': '4 3' }));
    root.appendChild(svgEl('text', { x: x + 5, y: top + 12, fill: '#ffb454', 'font-size': 11.5 }, m.label));
  });

  xs.forEach((x, i) => {
    let acc = 0;
    block.categories.forEach(c => {
      const v = c.values[i] || 0;
      if (v <= 0) return;
      const bh = (v / max) * plotH;
      const rect = svgEl('rect', {
        x: left + i * bw, y: top + plotH - acc - bh, width: Math.max(1, bw - 1), height: bh,
        fill: c.color
      });
      rect.appendChild(svgEl('title', null, `frame ${x} - ${c.label}: ${block.valueFormat ? block.valueFormat(v) : fmt(v)}`));
      root.appendChild(rect);
      acc += bh;
    });
  });

  block.categories.forEach((c, n) => {
    const ly = top + 14 + n * 17;
    root.appendChild(svgEl('rect', { x: left + plotW + 12, y: ly - 9, width: 15, height: 10, rx: 2, fill: c.color }));
    root.appendChild(svgEl('text', { x: left + plotW + 33, y: ly, fill: 'var(--dim)', 'font-size': 11.5 }, c.label));
  });

  root.appendChild(svgEl('text', { x: left, y: h - 10, fill: 'var(--dim)', 'font-size': 11.5 }, block.xLabel || ''));
  root.appendChild(svgEl('text', { x: left + plotW, y: h - 10, 'text-anchor': 'end', fill: 'var(--dim)', 'font-size': 11.5 }, fmtInt(xs[xs.length - 1])));
  return root;
}

/* A donut: the share of a whole. */
function chartDonut(block) {
  const size = 320, r = 118, inner = 74;
  const cx = size / 2, cy = size / 2;
  const total = block.items.reduce((a, i) => a + i.value, 0) || 1;

  const root = svgEl('svg', { viewBox: `0 0 ${size} ${size}`, role: 'img', style: 'max-width:340px;margin:0 auto' });

  let angle = -Math.PI / 2;
  block.items.forEach(item => {
    const sweep = (item.value / total) * Math.PI * 2;
    if (sweep <= 0) return;
    const a0 = angle, a1 = angle + sweep;
    angle = a1;
    const large = sweep > Math.PI ? 1 : 0;

    const x0 = cx + r * Math.cos(a0), y0 = cy + r * Math.sin(a0);
    const x1 = cx + r * Math.cos(a1), y1 = cy + r * Math.sin(a1);
    const xi1 = cx + inner * Math.cos(a1), yi1 = cy + inner * Math.sin(a1);
    const xi0 = cx + inner * Math.cos(a0), yi0 = cy + inner * Math.sin(a0);

    const d = `M ${x0} ${y0} A ${r} ${r} 0 ${large} 1 ${x1} ${y1} L ${xi1} ${yi1} A ${inner} ${inner} 0 ${large} 0 ${xi0} ${yi0} Z`;
    const seg = svgEl('path', { d, fill: item.color, stroke: 'var(--panel)', 'stroke-width': 1.5 });
    seg.appendChild(svgEl('title', null, `${item.label}: ${fmt(item.value / total * 100, 1)}%`));
    root.appendChild(seg);
  });

  return root;
}

/* A box plot: the distribution of one quantity over a set of frames. */
function chartBox(block) {
  const rowH = 26, top = 24, left = block.labelWidth || 250, right = 76;
  const h = top + block.items.length * rowH + 26;
  const plotW = CHART_W - left - right;
  const max = Math.max(...block.items.map(i => i.max), 1e-12);
  const X = v => left + (v / max) * plotW;

  const root = svgEl('svg', { viewBox: `0 0 ${CHART_W} ${h}`, role: 'img' });

  for (let g = 0; g <= 4; g++) {
    const x = left + (g / 4) * plotW;
    root.appendChild(svgEl('line', { x1: x, y1: top - 8, x2: x, y2: top + block.items.length * rowH, stroke: 'var(--line)' }));
    root.appendChild(svgEl('text', {
      x, y: top - 12, 'text-anchor': 'middle', fill: 'var(--dim)', 'font-size': 11,
      'font-family': 'Consolas, monospace'
    }, axisLabel(block, g, (g / 4) * max, 3)));
  }

  block.items.forEach((item, n) => {
    const y = top + n * rowH + rowH / 2;
    const color = item.color || 'var(--accent)';
    const boxH = 13;

    root.appendChild(svgEl('text', {
      x: left - 10, y: y + 4, 'text-anchor': 'end', fill: 'var(--ink)', 'font-size': 12.5
    }, item.label));

    root.appendChild(svgEl('line', { x1: X(item.min), y1: y, x2: X(item.max), y2: y, stroke: color, 'stroke-width': 1.4 }));
    root.appendChild(svgEl('line', { x1: X(item.min), y1: y - 5, x2: X(item.min), y2: y + 5, stroke: color, 'stroke-width': 1.4 }));
    root.appendChild(svgEl('line', { x1: X(item.max), y1: y - 5, x2: X(item.max), y2: y + 5, stroke: color, 'stroke-width': 1.4 }));

    const rect = svgEl('rect', {
      x: X(item.p25), y: y - boxH / 2, width: Math.max(1, X(item.p75) - X(item.p25)), height: boxH,
      fill: color, 'fill-opacity': 0.35, stroke: color, 'stroke-width': 1.2, rx: 2
    });
    rect.appendChild(svgEl('title', null,
      `${item.label}: min ${fmt(item.min, 2)}, p25 ${fmt(item.p25, 2)}, median ${fmt(item.median, 2)}, p75 ${fmt(item.p75, 2)}, max ${fmt(item.max, 2)}`));
    root.appendChild(rect);

    root.appendChild(svgEl('line', { x1: X(item.median), y1: y - boxH / 2, x2: X(item.median), y2: y + boxH / 2, stroke: color, 'stroke-width': 2.4 }));
    root.appendChild(svgEl('text', {
      x: left + plotW + 10, y: y + 4, fill: 'var(--dim)', 'font-size': 11.5,
      'font-family': 'Consolas, monospace'
    }, item.text || fmt(item.median)));
  });

  return root;
}

/* ------------------------------------------------------------------ blocks */

function renderBlock(block) {
  switch (block.kind) {
    case 'heading': return el('h2', null, block.text);
    case 'sub': return el('h3', null, block.text);

    case 'prose':
      return el('div', { html: block.html });

    case 'note':
      return el('p', { class: 'note', html: block.html });

    case 'cards': {
      const wrap = el('div', { class: 'cards' });
      block.items.forEach(i => {
        const card = el('div', { class: 'card' });
        card.appendChild(el('div', { class: 'k' }, i.label));
        const v = el('div', { class: 'v' }, i.value);
        if (i.unit) v.appendChild(el('span', { class: 'u' }, i.unit));
        card.appendChild(v);
        if (i.sub) card.appendChild(el('div', { class: 'k', style: 'margin-top:4px' }, i.sub));
        wrap.appendChild(card);
      });
      return wrap;
    }

    case 'code': {
      // The listing goes into a <code> inside the <pre>: `el` takes text, not a node, so the two
      // are built separately (passing the inner node as the text turned it into "[object ...]").
      const pre = el('pre');
      pre.appendChild(el('code', null, block.text));
      return pre;
    }

    case 'table': {
      const table = el('table');
      if (block.caption) table.appendChild(el('caption', null, block.caption));
      const thead = el('thead'), tr = el('tr');
      block.head.forEach(hd => tr.appendChild(el('th', { class: hd.n ? 'n' : '' }, hd.text !== undefined ? hd.text : hd)));
      thead.appendChild(tr);
      table.appendChild(thead);
      const tbody = el('tbody');
      block.rows.forEach(row => {
        const r = el('tr', { class: row.class || '' });
        (row.cells || row).forEach((cell, ci) => {
          const spec = block.head[ci] || {};
          const td = el('td', { class: spec.n ? 'n' : '' });
          if (cell && typeof cell === 'object') {
            if (cell.color) td.appendChild(el('span', { class: 'swatch', style: `background:${cell.color}` }));
            td.appendChild(document.createTextNode(cell.text !== undefined ? cell.text : ''));
            if (cell.sub) td.appendChild(el('span', { class: 'sub' }, ' ' + cell.sub));
          } else {
            td.textContent = cell === null || cell === undefined ? '' : String(cell);
          }
          r.appendChild(td);
        });
        tbody.appendChild(r);
      });
      table.appendChild(tbody);
      return table;
    }

    case 'chart': {
      const wrap = el('div', { class: 'chart' });
      if (block.title) wrap.appendChild(el('h3', null, block.title));
      if (block.caption) wrap.appendChild(el('p', { class: 'cap', html: block.caption }));

      const draw = () => {
        let node = null;
        if (block.chart === 'bars') node = chartBars(block);
        else if (block.chart === 'columns') node = chartColumns(block);
        else if (block.chart === 'lines') node = chartLines(block);
        else if (block.chart === 'stacked') node = chartStacked(block);
        else if (block.chart === 'donut') node = chartDonut(block);
        else if (block.chart === 'box') node = chartBox(block);
        if (node) wrap.appendChild(node);

        if (block.legend) {
          const lg = el('div', { class: 'legend' });
          block.legend.forEach(l => {
            const item = el('span');
            item.appendChild(el('i', { style: `background:${l.color}` }));
            item.appendChild(document.createTextNode(l.label));
            lg.appendChild(item);
          });
          wrap.appendChild(lg);
        }
        if (block.note) wrap.appendChild(el('p', { class: 'cap', html: block.note }));
      };

      if (block.after) {
        const schedule = () => { if (!wrap.dataset.drawn) { wrap.dataset.drawn = '1'; draw(); } };
        if ('requestIdleCallback' in window) requestIdleCallback(schedule); else setTimeout(schedule, 0);
      } else {
        draw();
      }
      return wrap;
    }

    default:
      return el('div', { class: 'note' }, 'unknown block: ' + block.kind);
  }
}

function main() {
  const report = window.REPORT;

  document.title = report.title || 'Guest frame profile';

  const header = el('header', { class: 'top' });
  header.appendChild(el('h1', null, report.title || 'Guest frame profile'));
  header.appendChild(el('div', { class: 'sub', html: report.subtitle || '' }));

  const wrap = el('div', { class: 'wrap' });
  wrap.appendChild(header);
  report.blocks.forEach(b => wrap.appendChild(renderBlock(b)));
  wrap.appendChild(el('footer', { html: report.footer || '' }));

  document.body.appendChild(wrap);
}

document.addEventListener('DOMContentLoaded', main);
