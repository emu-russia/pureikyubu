#!/usr/bin/env python3
"""
Generate a synthetic PowerPC (Gekko) workload for the standalone interpreter
benchmark.  The blob is loaded at physical 0x1000 and entered at 0x80001000.

The instruction mix is roughly what GameCube game code looks like:
integer ALU ~35%, loads ~20%, stores ~10%, branches ~14%, compare ~8%,
floating point ~8%, paired single ~5%.

Usage: gen_workload.py <out.bin> [--body N] [--seed S]
"""

import random
import struct
import sys

MASK32 = 0xFFFFFFFF


class Code:
    def __init__(self):
        self.words = []

    def emit(self, w):
        self.words.append(w & MASK32)

    # ---- D-form -----------------------------------------------------------
    def d(self, op, rt, ra, imm):
        self.emit((op << 26) | (rt << 21) | (ra << 16) | (imm & 0xFFFF))

    def addi(self, rt, ra, si):   self.d(14, rt, ra, si)
    def addis(self, rt, ra, si):  self.d(15, rt, ra, si)
    def mulli(self, rt, ra, si):  self.d(7, rt, ra, si)
    def subfic(self, rt, ra, si): self.d(8, rt, ra, si)
    def ori(self, ra, rs, ui):    self.d(24, rs, ra, ui)
    def oris(self, ra, rs, ui):   self.d(25, rs, ra, ui)
    def xori(self, ra, rs, ui):   self.d(26, rs, ra, ui)
    def andi_d(self, ra, rs, ui): self.d(28, rs, ra, ui)
    def andis_d(self, ra, rs, ui): self.d(29, rs, ra, ui)
    def lwz(self, rt, ra, d):     self.d(32, rt, ra, d)
    def lbz(self, rt, ra, d):     self.d(34, rt, ra, d)
    def stw(self, rs, ra, d):     self.d(36, rs, ra, d)
    def stb(self, rs, ra, d):     self.d(38, rs, ra, d)
    def lhz(self, rt, ra, d):     self.d(40, rt, ra, d)
    def lha(self, rt, ra, d):     self.d(42, rt, ra, d)
    def sth(self, rs, ra, d):     self.d(44, rs, ra, d)
    def lfs(self, frt, ra, d):    self.d(48, frt, ra, d)
    def lfd(self, frt, ra, d):    self.d(50, frt, ra, d)
    def stfs(self, frs, ra, d):   self.d(52, frs, ra, d)
    def stfd(self, frs, ra, d):   self.d(54, frs, ra, d)
    def cmpi(self, bf, ra, si):   self.emit((11 << 26) | (bf << 23) | (ra << 16) | (si & 0xFFFF))
    def cmpli(self, bf, ra, ui):  self.emit((10 << 26) | (bf << 23) | (1 << 22) | (ra << 16) | (ui & 0xFFFF))

    # ---- X-form (opcode 31) ----------------------------------------------
    def x(self, rt, ra, rb, xo, rc=0):
        self.emit((31 << 26) | (rt << 21) | (ra << 16) | (rb << 11) | (xo << 1) | rc)

    def add(self, rt, ra, rb, rc=0):  self.x(rt, ra, rb, 266, rc)
    def addc(self, rt, ra, rb, rc=0): self.x(rt, ra, rb, 10, rc)
    def adde(self, rt, ra, rb, rc=0): self.x(rt, ra, rb, 138, rc)
    def subf(self, rt, ra, rb, rc=0): self.x(rt, ra, rb, 40, rc)
    def neg(self, rt, ra, rc=0):      self.x(rt, ra, 0, 104, rc)
    def mullw(self, rt, ra, rb, rc=0): self.x(rt, ra, rb, 235, rc)
    def mulhw(self, rt, ra, rb, rc=0): self.x(rt, ra, rb, 75, rc)
    def divw(self, rt, ra, rb, rc=0): self.x(rt, ra, rb, 491, rc)
    def _and(self, ra, rs, rb, rc=0): self.x(rs, ra, rb, 28, rc)
    def _or(self, ra, rs, rb, rc=0):  self.x(rs, ra, rb, 444, rc)
    def _xor(self, ra, rs, rb, rc=0): self.x(rs, ra, rb, 316, rc)
    def nand(self, ra, rs, rb, rc=0): self.x(rs, ra, rb, 476, rc)
    def nor(self, ra, rs, rb, rc=0):  self.x(rs, ra, rb, 124, rc)
    def andc(self, ra, rs, rb, rc=0): self.x(rs, ra, rb, 60, rc)
    def slw(self, ra, rs, rb, rc=0):  self.x(rs, ra, rb, 24, rc)
    def srw(self, ra, rs, rb, rc=0):  self.x(rs, ra, rb, 536, rc)
    def sraw(self, ra, rs, rb, rc=0): self.x(rs, ra, rb, 792, rc)
    def cntlzw(self, ra, rs, rc=0):   self.x(rs, ra, 0, 26, rc)
    def extsb(self, ra, rs, rc=0):    self.x(rs, ra, 0, 954, rc)
    def extsh(self, ra, rs, rc=0):    self.x(rs, ra, 0, 922, rc)
    def lwzx(self, rt, ra, rb):       self.x(rt, ra, rb, 23)
    def lhzx(self, rt, ra, rb):       self.x(rt, ra, rb, 279)
    def lbzx(self, rt, ra, rb):       self.x(rt, ra, rb, 87)
    def lhax(self, rt, ra, rb):       self.x(rt, ra, rb, 343)
    def stwx(self, rs, ra, rb):       self.x(rs, ra, rb, 151)
    def sthx(self, rs, ra, rb):       self.x(rs, ra, rb, 407)
    def stbx(self, rs, ra, rb):       self.x(rs, ra, rb, 215)
    def cmpw(self, bf, ra, rb):       self.emit((31 << 26) | (bf << 23) | (ra << 16) | (rb << 11) | (0 << 1))
    def cmplw(self, bf, ra, rb):      self.emit((31 << 26) | (bf << 23) | (1 << 22) | (ra << 16) | (rb << 11) | (32 << 1))
    def mfspr(self, rt, spr):
        # SPR[0:4] sits in the RA field, SPR[5:9] in the RB field.
        self.x(rt, spr & 0x1F, (spr >> 5) & 0x1F, 339)
    def mtspr(self, spr, rs):
        self.x(rs, spr & 0x1F, (spr >> 5) & 0x1F, 467)
    def mftb(self, rt, tbr):
        self.x(rt, tbr & 0x1F, (tbr >> 5) & 0x1F, 371)

    # ---- Rotate (opcode 20/21/23) ----------------------------------------
    def rlwinm(self, ra, rs, sh, mb, me, rc=0):
        self.emit((21 << 26) | (rs << 21) | (ra << 16) | (sh << 11) | (mb << 6) | (me << 1) | rc)
    def rlwimi(self, ra, rs, sh, mb, me, rc=0):
        self.emit((20 << 26) | (rs << 21) | (ra << 16) | (sh << 11) | (mb << 6) | (me << 1) | rc)
    def rlwnm(self, ra, rs, rb, mb, me, rc=0):
        self.emit((23 << 26) | (rs << 21) | (ra << 16) | (rb << 11) | (mb << 6) | (me << 1) | rc)
    def srawi(self, ra, rs, sh, rc=0):
        self.emit((31 << 26) | (rs << 21) | (ra << 16) | (sh << 11) | (824 << 1) | rc)

    # ---- Branches ---------------------------------------------------------
    def b(self, off):     self.emit((18 << 26) | (off & 0x03FFFFFC))
    def bl(self, off):    self.emit((18 << 26) | (off & 0x03FFFFFC) | 1)
    def bc(self, bo, bi, off): self.emit((16 << 26) | (bo << 21) | (bi << 16) | (off & 0xFFFC))
    def bdnz(self, off):  self.bc(16, 0, off)
    def bne(self, crb, off): self.bc(4, crb, off)
    def beq(self, crb, off): self.bc(12, crb, off)
    def bgt(self, crb, off): self.bc(12, crb + 1, off)
    def bclr(self):       self.emit((19 << 26) | (20 << 21) | (16 << 1))

    # ---- FP (opcode 63 / 59) ---------------------------------------------
    def a(self, op, frt, fra, frb, frc, xo, rc=0):
        self.emit((op << 26) | (frt << 21) | (fra << 16) | (frb << 11) | (frc << 6) | (xo << 1) | rc)

    def fadd(self, frt, fra, frb, rc=0):  self.a(63, frt, fra, frb, 0, 21, rc)
    def fadds(self, frt, fra, frb, rc=0): self.a(59, frt, fra, frb, 0, 21, rc)
    def fsub(self, frt, fra, frb, rc=0):  self.a(63, frt, fra, frb, 0, 20, rc)
    def fmul(self, frt, fra, frc, rc=0):  self.a(63, frt, fra, 0, frc, 25, rc)
    def fmuls(self, frt, fra, frc, rc=0): self.a(59, frt, fra, 0, frc, 25, rc)
    def fdiv(self, frt, fra, frb, rc=0):  self.a(63, frt, fra, frb, 0, 18, rc)
    def fmadd(self, frt, fra, frc, frb, rc=0): self.a(63, frt, fra, frb, frc, 29, rc)
    def frsp(self, frt, frb, rc=0):       self.a(63, frt, 0, frb, 0, 12, rc)
    def fcmpu(self, bf, fra, frb):        self.a(63, bf, fra, frb, 0, 0)
    def fcmpo(self, bf, fra, frb):        self.a(63, bf, fra, frb, 0, 32)
    def fmr(self, frt, frb, rc=0):        self.a(63, frt, 0, frb, 0, 72, rc)
    def fneg(self, frt, frb, rc=0):       self.a(63, frt, 0, frb, 0, 40, rc)

    # ---- Paired single (opcode 4) ----------------------------------------
    def ps(self, frt, fra, frb, frc, xo, rc=0):
        self.emit((4 << 26) | (frt << 21) | (fra << 16) | (frb << 11) | (frc << 6) | (xo << 1) | rc)

    def ps_add(self, d, a, b, rc=0):    self.ps(d, a, b, 0, 21, rc)
    def ps_sub(self, d, a, b, rc=0):    self.ps(d, a, b, 0, 20, rc)
    def ps_mul(self, d, a, c, rc=0):    self.ps(d, a, 0, c, 25, rc)
    def ps_madd(self, d, a, c, b, rc=0): self.ps(d, a, b, c, 29, rc)
    def ps_merge00(self, d, a, b, rc=0): self.ps(d, a, b, 0, 528, rc)
    def ps_sum0(self, d, a, b, c, rc=0): self.ps(d, a, b, c, 10, rc)

    # ---- misc -------------------------------------------------------------
    def sync(self): self.emit(0x7C0004AC)
    def nop(self):  self.emit(0x60000000)


GP = [f"r{i}" for i in range(32)]
FP = [f"f{i}" for i in range(32)]


def generate(body_instrs, seed, mix="full", window_mb=4):
    rnd = random.Random(seed)
    c = Code()

    # Integer registers used as "live" data (avoid r27-r31 which are pointers).
    live = list(range(2, 26))   # r26 is a fixed small index register
    flive = list(range(0, 16))

    def r():  return rnd.choice(live)
    def r2(): 
        a = rnd.choice(live)
        b = rnd.choice(live)
        return a, b
    def f():  return rnd.choice(flive)

    def mem_disp(size):
        # displacement inside a 32 KB window, aligned and inside RAM
        return rnd.randrange(0, 32768 - size) & ~(size - 1)

    ops = []

    # Prologue: r31 = data base (effective 0x80800000), r28 = offset, r27 = ptr
    c.addis(31, 0, 0x8080)          # r31 = 0x80800000
    c.addi(28, 0, 0)                # offset
    c.addi(26, 0, 0x1000)           # fixed small index for the X-form accesses
    c.addi(30, 0, 0)                # loop counter / cratch
    c.add(27, 31, 28)               # ptr = base + offset

    body_start = len(c.words)

    if mix == "one":
        weights = [("one", 1)]
    elif mix == "alu":
        weights = [("alu", 70), ("cmp", 14), ("branch", 14), ("misc", 2)]
    elif mix == "mem":
        weights = [("alu", 20), ("load", 30), ("store", 20), ("branch", 14), ("cmp", 8), ("misc", 8)]
    else:
        weights = [
            ("alu",      34),
            ("load",     19),
            ("store",    10),
            ("branch",   14),
            ("cmp",       8),
            ("fp",        9),
            ("ps",        5),
            ("misc",      6),
        ]
    total = sum(w for _, w in weights)
    table = []
    for name, w in weights:
        table += [name] * w

    for i in range(body_instrs):
        k = rnd.choice(table)
        rt = r()
        a, b = r2()

        if k == "one":
            c.add(3, 4, 5)
        elif k == "alu":
            choice = rnd.randrange(0, 14)
            if choice == 0:   c.add(rt, a, b, rnd.randrange(0, 4) == 0)
            elif choice == 1: c.addi(rt, a, rnd.randrange(-32768, 32767))
            elif choice == 2: c.addis(rt, a, rnd.randrange(-32768, 32767))
            elif choice == 3: c._or(rt, a, b, rnd.randrange(0, 4) == 0)
            elif choice == 4: c._and(rt, a, b, rnd.randrange(0, 4) == 0)
            elif choice == 5: c._xor(rt, a, b)
            elif choice == 6: c.subf(rt, a, b)
            elif choice == 7: c.rlwinm(rt, a, rnd.randrange(0, 32), rnd.randrange(0, 32), rnd.randrange(0, 32), rnd.randrange(0, 4) == 0)
            elif choice == 8: c.slw(rt, a, b)
            elif choice == 9: c.srw(rt, a, b)
            elif choice == 10: c.srawi(rt, a, rnd.randrange(0, 32))
            elif choice == 11: c.mullw(rt, a, b)
            elif choice == 12: c.ori(rt, a, rnd.randrange(0, 65536))
            else: c.cntlzw(rt, a)
        elif k == "load":
            choice = rnd.randrange(0, 6)
            if choice == 0:   c.lwz(rt, 27, mem_disp(4))
            elif choice == 1: c.lwzx(rt, 27, 26)
            elif choice == 2: c.lhz(rt, 27, mem_disp(2))
            elif choice == 3: c.lbz(rt, 27, mem_disp(1))
            elif choice == 4: c.lha(rt, 27, mem_disp(2))
            else:             c.lwz(rt, 27, mem_disp(4))
        elif k == "store":
            choice = rnd.randrange(0, 5)
            if choice == 0:   c.stw(a, 27, mem_disp(4))
            elif choice == 1: c.stwx(a, 27, 26)
            elif choice == 2: c.sth(a, 27, mem_disp(2))
            elif choice == 3: c.stb(a, 27, mem_disp(1))
            else:             c.stw(a, 27, mem_disp(4))
        elif k == "cmp":
            crf = rnd.randrange(0, 8)
            choice = rnd.randrange(0, 4)
            if choice == 0:   c.cmpi(crf, a, rnd.randrange(-32768, 32767))
            elif choice == 1: c.cmpli(crf, a, rnd.randrange(0, 65535))
            elif choice == 2: c.cmpw(crf, a, b)
            else:             c.cmplw(crf, a, b)
        elif k == "fp":
            choice = rnd.randrange(0, 8)
            d = f()
            if choice == 0:   c.fadd(d, f(), f())
            elif choice == 1: c.fadds(d, f(), f())
            elif choice == 2: c.fsub(d, f(), f())
            elif choice == 3: c.fmul(d, f(), f())
            elif choice == 4: c.fmuls(d, f(), f())
            elif choice == 5: c.fmadd(d, f(), f(), f())
            elif choice == 6: c.frsp(d, f())
            else:             c.fcmpu(rnd.randrange(0, 8), f(), f())
        elif k == "ps":
            choice = rnd.randrange(0, 6)
            d = f()
            if choice == 0:   c.ps_add(d, f(), f())
            elif choice == 1: c.ps_sub(d, f(), f())
            elif choice == 2: c.ps_mul(d, f(), f())
            elif choice == 3: c.ps_madd(d, f(), f(), f())
            elif choice == 4: c.ps_merge00(d, f(), f())
            else:             c.ps_sum0(d, f(), f(), f())
        elif k == "branch":
            choice = rnd.randrange(0, 8)
            if choice <= 4:   c.bc(4, rnd.randrange(0, 32), 8)     # branch if CR bit clear
            elif choice == 5: c.bc(12, rnd.randrange(0, 32), 8)    # branch if CR bit set
            elif choice == 6: c.bc(4, rnd.randrange(0, 32), 12)
            else:             c.bc(12, rnd.randrange(0, 32), 12)
        elif k == "misc":
            choice = rnd.randrange(0, 6)
            if choice == 0:   c.mfspr(rt, 1)          # XER
            elif choice == 1: c.mfspr(rt, 8)          # LR
            elif choice == 2: c.mftb(rt, 268)         # TBL
            elif choice == 3: c.extsb(rt, a)
            elif choice == 4: c.extsh(rt, a)
            else:             c.nor(rt, a, b)

    body_end = len(c.words)

    # Loop tail: advance the pointer, close the loop.
    c.addi(28, 28, 64)
    # offset = offset & (window - 1); MB=32-window_bits
    wbits = (window_mb * 1024 * 1024).bit_length() - 1
    c.rlwinm(28, 28, 0, 32 - wbits, 31)
    c.add(27, 31, 28)               # ptr = base + offset
    c.addi(30, 30, 1)
    c.cmpi(0, 30, 0)                # CR0 = (counter == 0)
    c.beq(2, 4)                     # not taken in practice
    c.beq(2, 8)                     # not taken in practice
    b_idx = len(c.words)
    c.b(0)
    off = (body_start - b_idx) * 4
    c.words[b_idx] = (18 << 26) | (off & 0x03FFFFFC)

    return c.words, body_start


def main():
    out = sys.argv[1]
    body = 2000
    seed = 12345
    mix = "full"
    window_mb = 4
    args = sys.argv[2:]
    i = 0
    while i < len(args):
        if args[i] == "--body":
            body = int(args[i + 1]); i += 2
        elif args[i] == "--seed":
            seed = int(args[i + 1]); i += 2
        elif args[i] == "--mix":
            mix = args[i + 1]; i += 2
        elif args[i] == "--window":
            window_mb = int(args[i + 1]); i += 2
        else:
            i += 1

    words, body_start = generate(body, seed, mix, window_mb)
    data = b"".join(struct.pack(">I", w) for w in words)
    with open(out, "wb") as f:
        f.write(data)
    print(f"wrote {out}: {len(words)} instructions ({len(data)} bytes), entry 0x80001000, body starts at word {body_start}")


if __name__ == "__main__":
    main()
