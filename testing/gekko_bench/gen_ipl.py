#!/usr/bin/env python3
"""
Generate a synthetic IPL (boot ROM) plus a small "game" program, so that the
boot path can be exercised without a real boot ROM image.

bootrom.bin  - placed at 0xFFF00100, executed with translation OFF
game.bin     - placed at 0x80001000, executed after the IPL has enabled BATs,
               the caches and address translation

The IPL deliberately does what the real one does:
  * writes the IBAT/DBAT registers,
  * enables the instruction and data caches through HID0,
  * enables MSR[IR]/[DR] through mtmsr,
  * flushes an instruction range with dcbst + icbi + sync per cache line
    (this is the loop that made the boot slow when one icbi cost a 16K entry walk),
  * enters the game with bcctr.

The game then raises a DSI on purpose, so that the exception vector, rfi and the
translation-mode switch are exercised too.
"""

import struct
import sys

MASK = 0xFFFFFFFF


class Code:
    def __init__(self):
        self.words = []

    def emit(self, w):
        self.words.append(w & MASK)

    def d(self, op, rt, ra, imm):
        self.emit((op << 26) | (rt << 21) | (ra << 16) | (imm & 0xFFFF))

    def addi(self, rt, ra, si):   self.d(14, rt, ra, si)
    def addis(self, rt, ra, si):  self.d(15, rt, ra, si)
    def addic_d(self, rt, ra, si): self.d(13, rt, ra, si)
    def ori(self, ra, rs, ui):    self.d(24, rs, ra, ui)
    def oris(self, ra, rs, ui):   self.d(25, rs, ra, ui)
    def lwz(self, rt, ra, d):     self.d(32, rt, ra, d)
    def stw(self, rs, ra, d):     self.d(36, rs, ra, d)

    def x(self, rt, ra, rb, xo, rc=0):
        self.emit((31 << 26) | (rt << 21) | (ra << 16) | (rb << 11) | (xo << 1) | rc)

    def add(self, rt, ra, rb, rc=0): self.x(rt, ra, rb, 266, rc)

    def mfspr(self, rt, spr):
        self.x(rt, spr & 0x1F, (spr >> 5) & 0x1F, 339)

    def mtspr(self, spr, rs):
        self.x(rs, spr & 0x1F, (spr >> 5) & 0x1F, 467)

    def bc(self, bo, bi, off):
        self.emit((16 << 26) | (bo << 21) | (bi << 16) | (off & 0xFFFC))

    def bctr(self):
        # XO 528 is bcctr; XO 16 would be bclr.
        self.emit((19 << 26) | (20 << 21) | (528 << 1))

    def isync(self): self.emit(0x4C00012C)
    def sync(self):  self.emit(0x7C0004AC)
    def dcbst(self, ra, rb): self.emit((31 << 26) | (ra << 16) | (rb << 11) | (54 << 1))
    def icbi(self, ra, rb):  self.emit((31 << 26) | (ra << 16) | (rb << 11) | (982 << 1))


# SPRs
DBAT0U, DBAT0L, DBAT1U, DBAT1L = 536, 537, 538, 539
IBAT0U, IBAT0L = 528, 529
IBAT1U, IBAT1L = 530, 531
HID0 = 1008
SRR0, SRR1 = 26, 27
LR = 8


def bootrom(flushLines):
    c = Code()

    # DBAT0: 0x80000000, 256 MB, write-back cached  (0x80001FFF / 0x00000002)
    c.addis(3, 0, 0x8000); c.ori(3, 3, 0x1FFF)
    c.mtspr(DBAT0U, 3)
    c.addi(3, 0, 2)
    c.mtspr(DBAT0L, 3)

    # IBAT0: the same range
    c.addis(3, 0, 0x8000); c.ori(3, 3, 0x1FFF)
    c.mtspr(IBAT0U, 3)
    c.addi(3, 0, 2)
    c.mtspr(IBAT0L, 3)

    # BAT1: identity map of the 0xF0000000 block (256 MB, which is where the boot ROM
    # lives), cache inhibited and guarded, so that the IPL stays fetchable once
    # MSR[IR] is set. BEPI has to be aligned to the block size, so it is 0xF0000000
    # and not 0xFFF00000.
    c.addis(3, 0, 0xF000); c.ori(3, 3, 0x1FFF)
    c.mtspr(DBAT1U, 3)
    c.mtspr(IBAT1U, 3)
    c.addis(3, 0, 0xF000); c.ori(3, 3, 0x0028)
    c.mtspr(DBAT1L, 3)
    c.mtspr(IBAT1L, 3)
    c.isync()

    # Enable the instruction and data caches (HID0[ICE] and HID0[DCE])
    c.mfspr(3, HID0)
    c.oris(3, 3, 0xC800)
    c.mtspr(HID0, 3)
    c.isync()

    # Enable address translation (MSR[IR] | MSR[DR])
    c.emit(0x7C0000A6)                 # mfmsr r0
    c.ori(0, 0, 0x30)
    c.emit(0x7C000124)                 # mtmsr r0
    c.isync()

    # Flush `flushLines` cache lines of the loaded code: dcbst + icbi + sync each.
    c.addis(4, 0, 0x8000); c.ori(4, 4, 0x1000)
    if flushLines < 32768:
        c.addi(5, 0, flushLines)
    else:
        c.addis(5, 0, (flushLines >> 16) & 0xFFFF)
        if flushLines & 0xFFFF:
            c.ori(5, 5, flushLines & 0xFFFF)
    loop = len(c.words)
    c.dcbst(0, 4)
    c.icbi(0, 4)
    c.sync()
    c.addi(4, 4, 32)
    c.addic_d(5, 5, -1)                # addic. r5, r5, -1
    off = (loop - len(c.words)) * 4
    c.bc(4, 2, off)                    # bne loop
    c.isync()

    # Enter the game: bctr to 0x80001000
    c.addis(4, 0, 0x8000); c.ori(4, 4, 0x1000)
    c.mtspr(9, 4)                      # mtspr CTR, r4
    c.bctr()

    return c.words


def game(iterations):
    c = Code()

    # A little arithmetic loop over memory at 0x80100000, then a deliberate fault.
    c.addis(10, 0, 0x8010)             # r10 = data base
    c.addi(11, 0, 0)                   # counter
    loop = len(c.words)
    c.lwz(12, 10, 0)
    c.add(12, 12, 11)
    c.stw(12, 10, 0)
    c.addi(11, 11, 1)
    c.addic_d(0, 11, 0)                # addic. r0, r11, 0  (sets CR0)
    off = (loop - len(c.words)) * 4
    c.bc(4, 2, off)                    # bne loop

    # Deliberate DSI: 0x10000000 is not covered by any BAT and segment 0 is a
    # direct-store segment, so the access raises a DSI. The vector at 0x300 skips
    # the instruction and returns with rfi, which flips MSR[IR]/[DR].
    c.addis(13, 0, 0x1000)
    c.lwz(14, 13, 0)

    # Keep going after the handler returns.
    c.addi(11, 0, 0)
    tail = len(c.words)
    c.addi(11, 11, 1)
    c.addic_d(0, 11, 0)
    off = (tail - len(c.words)) * 4
    c.bc(4, 2, off)

    return c.words


def ipl2():
    """A stand-in for the second stage IPL: uses bclrl as a "call through LR" thunk,
    which is exactly the idiom the real IPL2 entry code uses."""
    c = Code()

    c.addis(3, 0, 0x8080); c.ori(3, 3, 0x0000)   # data pointer
    c.addi(5, 0, 200)                            # loop counter

    loop = len(c.words)
    call = len(c.words)
    c.emit(0)                                    # place holder for 'bl thunk'
    # ret1:
    c.lwz(6, 3, 0)
    c.addi(6, 6, 1)
    c.stw(6, 3, 0)
    c.addic_d(5, 5, -1)                          # addic. r5, r5, -1
    off = (loop - len(c.words)) * 4
    c.bc(4, 2, off)                              # bne loop
    c.emit(0)                                    # place holder for 'b _start'

    thunk = len(c.words)
    # bclrl: LR_new = pc+4, pc = LR_old. XO 16 with LK=1, BO=20 (always).
    c.emit((19 << 26) | (20 << 21) | ((16 << 1) | 1))
    c.emit(0x60000000)                           # the instruction after the thunk

    # patch the call: bl thunk
    c.words[call] = (18 << 26) | 1 | (((thunk - call) * 4) & 0x03FFFFFC)
    # patch the restart
    c.words[thunk - 1] = (18 << 26) | ((0 - (thunk - 1)) * 4 & 0x03FFFFFC)

    return c.words


def main():
    out = sys.argv[1]
    lines = int(sys.argv[2]) if len(sys.argv) > 2 else 64
    iters = int(sys.argv[3]) if len(sys.argv) > 3 else 1000

    with open(out + "_bootrom.bin", "wb") as f:
        f.write(b"".join(struct.pack(">I", w) for w in bootrom(lines)))
    with open(out + "_game.bin", "wb") as f:
        f.write(b"".join(struct.pack(">I", w) for w in game(iters)))
    with open(out + "_ipl2.bin", "wb") as f:
        f.write(b"".join(struct.pack(">I", w) for w in ipl2()))
    print("wrote %s_bootrom.bin, %s_game.bin and %s_ipl2.bin" % (out, out, out))


if __name__ == "__main__":
    main()
