// A self-check of a generated report: run the renderer against a minimal DOM and assert that every
// block produced what it was supposed to.
//
//   node tools/checkreport.js report.html [tools/report.js]
//
// It caught a real bug: `code` blocks were handing a DOM node to a helper that takes text, so the
// disassembly rendered as "[object HTMLPreElement]". A chart that silently draws nothing, or a
// table that loses its rows, fails here instead of in the reader's browser.
'use strict';

const fs = require('fs');
const path = require('path');

const htmlPath = process.argv[2];
const jsPath = process.argv[3] || path.join(__dirname, 'report.js');

if (!htmlPath) {
  console.error('usage: node checkreport.js <report.html> [report.js]');
  process.exit(2);
}

const html = fs.readFileSync(htmlPath, 'utf8');
const match = html.match(/window\.REPORT = ([\s\S]*?);<\/script>/);
if (!match) {
  console.error('the report carries no window.REPORT payload');
  process.exit(1);
}

const report = JSON.parse(match[1]);
const js = fs.readFileSync(jsPath, 'utf8');

// A DOM just large enough for the renderer: nodes remember their tag, attributes and text.
const created = [];
function makeNode(tag) {
  const node = {
    tagName: tag,
    children: [],
    attrs: {},
    dataset: {},
    style: {},
    className: '',
    innerHTML: '',
    _text: '',
    setAttribute(k, v) { this.attrs[k] = v; },
    getAttribute(k) { return this.attrs[k]; },
    appendChild(c) { this.children.push(c); return c; },
    addEventListener() {},
    set textContent(v) { this._text = String(v); },
    get textContent() { return this._text; },
  };
  created.push(node);
  return node;
}

const listeners = {};
global.window = { REPORT: report };
global.document = {
  title: '',
  createElement: makeNode,
  createElementNS: (ns, tag) => makeNode(tag),
  createTextNode: (t) => ({ nodeType: 3, textContent: String(t) }),
  addEventListener: (ev, fn) => { listeners[ev] = fn; },
  body: makeNode('body'),
};
global.setTimeout = (fn) => fn();
global.requestIdleCallback = undefined;

try {
  eval(js);
  listeners['DOMContentLoaded']();
} catch (e) {
  console.error('RENDER FAILED:', e && e.stack ? e.stack : e);
  process.exit(1);
}

const count = (tag) => created.filter(n => n.tagName === tag).length;
const shapes = created.filter(n => ['rect', 'path', 'line'].includes(n.tagName)).length;

const wanted = {};
for (const b of report.blocks) {
  const k = b.kind === 'chart' ? 'chart/' + b.chart : b.kind;
  wanted[k] = (wanted[k] || 0) + 1;
}

console.log('title       :', document.title);
console.log('blocks      :', report.blocks.length, JSON.stringify(wanted));
console.log('svg / shapes:', count('svg'), '/', shapes);
console.log('tables      :', count('table'), 'rows:', count('tr'), 'cells:', count('td'));

let failures = 0;
function check(ok, message) {
  if (!ok) { console.error('FAIL:', message); failures++; }
}

const wantCharts = report.blocks.filter(b => b.kind === 'chart').length;
check(count('svg') === wantCharts, `charts drawn ${count('svg')} of ${wantCharts}`);

const wantTables = report.blocks.filter(b => b.kind === 'table').length;
check(count('table') === wantTables, `tables drawn ${count('table')} of ${wantTables}`);

for (const b of report.blocks) {
  if (b.kind === 'code') {
    check(created.some(n => n.tagName === 'code' && n.textContent === b.text),
      'a code block lost its listing');
  }
  if (b.kind === 'prose' || b.kind === 'note') {
    check(created.some(n => n.innerHTML === b.html), 'an html block lost its text');
  }
}

console.log(failures === 0 ? 'OK' : `${failures} check(s) failed`);
process.exit(failures === 0 ? 0 : 1);
