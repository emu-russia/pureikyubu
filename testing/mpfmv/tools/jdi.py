#!/usr/bin/env python3
"""A minimal MCP client for the emulator's debug interface (src/mcp.cpp, wiki/mcp.md).

    python jdi.py "<image>" tools
    python jdi.py "<image>" call <tool> [key=value ...]
    python jdi.py "<image>" disasm <address> <count>

The emulator is started with `--mcp <image>` and driven over stdin/stdout with one JSON-RPC
message per line. stdout belongs to the protocol in that mode, so the emulator's own log goes to
stderr and is dropped here.

`disasm` is the convenience the research needed: `GekkoDisasm` takes one address at a time, so the
call is repeated for every instruction of a range.
"""

import argparse
import json
import os
import subprocess
import sys
import threading
import time

MPFMV = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(MPFMV))
DEFAULT_EMU = os.path.join(REPO, 'scripts', 'VS2026', 'x64', 'Release', 'pureikyubu_headless.exe')
BUILD = os.path.join(REPO, 'build')


class Mcp:
    def __init__(self, exe, image, cwd):
        self.proc = subprocess.Popen(
            [exe, '--mcp', image],
            cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, bufsize=1)
        self.next_id = 0

        # The handshake: the protocol revision the client speaks, then the notification that it is
        # ready for tool calls.
        reply = self.request('initialize', {
            'protocolVersion': '2025-06-18',
            'capabilities': {},
            'clientInfo': {'name': 'mpfmv', 'version': '1'},
        })
        self.server = reply.get('serverInfo', {}).get('name', '?')
        self.notify('notifications/initialized')

    def _send(self, obj):
        self.proc.stdin.write(json.dumps(obj) + '\n')
        self.proc.stdin.flush()

    def _recv(self):
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise SystemExit('the emulator closed the connection')
            line = line.strip()
            if not line:
                continue
            try:
                return json.loads(line)
            except json.JSONDecodeError:
                continue

    def request(self, method, params):
        self.next_id += 1
        self._send({'jsonrpc': '2.0', 'id': self.next_id, 'method': method, 'params': params})
        while True:
            msg = self._recv()
            if msg.get('id') == self.next_id:
                if 'error' in msg:
                    raise SystemExit('rpc error: %s' % msg['error'])
                return msg.get('result', {})

    def notify(self, method, params=None):
        self._send({'jsonrpc': '2.0', 'method': method, 'params': params or {}})

    def tools(self):
        return self.request('tools/list', {}).get('tools', [])

    def call(self, name, arguments):
        result = self.request('tools/call', {'name': name, 'arguments': arguments})
        out = []
        for item in result.get('content', []):
            out.append(item.get('text', ''))
        return '\n'.join(out), result.get('isError', False)

    def close(self):
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=20)
        except Exception:
            self.proc.kill()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('image')
    ap.add_argument('mode', choices=['tools', 'call', 'disasm', 'profiler'])
    ap.add_argument('rest', nargs='*')
    ap.add_argument('--emu', default=DEFAULT_EMU)
    args = ap.parse_args()

    image = os.path.abspath(args.image)
    mcp = Mcp(os.path.abspath(args.emu), image, BUILD)

    try:
        if args.mode == 'tools':
            for tool in mcp.tools():
                print('%-28s %s' % (tool['name'], tool.get('description', '')[:90]))
            return

        if args.mode == 'profiler':
            # `profiler <out.json> [seconds]`: run the emulator's own sampling profiler (it reads
            # `regs.pc` every couple of emulated milliseconds) and wait. The document is written
            # when the profiler is stopped, so it has to be stopped before the emulator goes away.
            if not args.rest:
                raise SystemExit('profiler needs an output file')
            out = os.path.abspath(args.rest[0])
            seconds = float(args.rest[1]) if len(args.rest) > 1 else 240.0

            if os.path.exists(out):
                os.remove(out)

            print('profiling for %.0f s...' % seconds, flush=True)
            mcp.call('StartProfiler', {'args': [out, '2']})
            time.sleep(seconds)
            mcp.call('StopProfiler', {})

            if not os.path.exists(out):
                raise SystemExit('the emulator wrote no profile to %s' % out)
            print('wrote %s (%d bytes)' % (out, os.path.getsize(out)))
            return

        if args.mode == 'call':
            name = args.rest[0]
            arguments = {}
            for item in args.rest[1:]:
                key, _, value = item.partition('=')
                arguments[key] = value
            text, is_error = mcp.call(name, arguments)
            print(text)
            if is_error:
                sys.exit(1)
            return

        if args.mode == 'disasm':
            address = int(args.rest[0], 0)
            count = int(args.rest[1]) if len(args.rest) > 1 else 32

            # Find the tool that disassembles one Gekko instruction.
            names = [t['name'] for t in mcp.tools()]
            tool = next((n for n in names if n.lower().endswith('gekkodisasm') or n == 'GekkoDisasm'), None)
            if tool is None:
                raise SystemExit('no disassembler tool; have: %s' % ', '.join(names))

            # Work out the argument name from the tool's schema.
            spec = next(t for t in mcp.tools() if t['name'] == tool)
            props = list((spec.get('inputSchema', {}).get('properties') or {'vaddr': {}}).keys())
            key = props[0] if props else 'vaddr'

            pc = address
            for _ in range(count):
                text, _err = mcp.call(tool, {key: '0x%08X' % pc})
                line = text.strip()

                # The answer is the tool's own JSON: { "result": [ "<mnemonic>" ] }.
                try:
                    parsed = json.loads(line)
                    if isinstance(parsed, dict) and isinstance(parsed.get('result'), list):
                        line = ' '.join(str(p) for p in parsed['result'])
                except json.JSONDecodeError:
                    pass

                print('0x%08X  %s' % (pc, line))
                pc += 4
            return
    finally:
        mcp.close()


if __name__ == '__main__':
    main()
