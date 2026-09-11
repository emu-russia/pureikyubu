#!/usr/bin/env python3
"""
Self-modifying-code probe for the Gekko interpreter.

The program warms up the decode cache on the `victim` instruction, rewrites that
instruction in memory, flushes the data cache and invalidates the instruction
cache, and then executes the same address again.

If the emulator serves a stale decode (or a stale instruction fetch), r5 ends up
as 0 instead of 0x1111.

Two variants are emitted (the harness picks one):
  variant 0 - full dance: stw + dcbst + sync + icbi + isync  -> r5 must be 0x1111
  variant 1 - no flush at all                                 -> r5 must stay 0
"""

import struct
import sys

MASK = 0xFFFFFFFF


def emit(words, w):
    words.append(w & MASK)


def d(op, rt, ra, imm):
    return (op << 26) | (rt << 21) | (ra << 16) | (imm & 0xFFFF)


def x(rt, ra, rb, xo, rc=0):
    return (31 << 26) | (rt << 21) | (ra << 16) | (rb << 11) | (xo << 1) | rc


def build(variant):
    w = []

    # --- prologue: warm up the decode cache on the victim instruction --------
    emit(w, 0x48000005)             # bl +4   (LR = address of the next word)
    emit(w, 0x7C6802A6)             # mflr r3  -> r3 = 0x80001004
    emit(w, d(14, 3, 3, 12))        # addi r3, r3, 12 -> &victim
    emit(w, d(14, 10, 0, 0))        # li   r10, 0

    loop_back = len(w)
    victim_index = len(w)
    emit(w, 0x38A00000)             # victim: li r5, 0
    emit(w, d(14, 10, 10, 1))       # addi r10, r10, 1
    emit(w, 0x2C0A0064)             # cmpi cr0, r10, 100
    # bne cr0, loop
    bne_index = len(w)
    emit(w, 0)
    off = (loop_back - bne_index) * 4
    w[bne_index] = (16 << 26) | (4 << 21) | (2 << 16) | (off & 0xFFFC)

    if variant == 0:
        # --- rewrite the victim instruction ---------------------------------
        emit(w, 0x3C8038A0)         # lis  r4, 0x38A0
        emit(w, 0x60841111)         # ori  r4, r4, 0x1111  -> "li r5, 0x1111"
        emit(w, 0x90830000)         # stw  r4, 0(r3)
        emit(w, 0x7C00186C)         # dcbst 0, r3
        emit(w, 0x7C0004AC)         # sync
        emit(w, 0x7C001FAC)         # icbi 0, r3
        emit(w, 0x4C00012C)         # isync

    # --- run the victim again ------------------------------------------------
    b_index = len(w)
    emit(w, 0)
    off = (victim_index - b_index) * 4
    w[b_index] = (18 << 26) | (off & 0x03FFFFFC)

    return w


def main():
    out = sys.argv[1]
    variant = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    words = build(variant)
    with open(out, "wb") as f:
        f.write(b"".join(struct.pack(">I", x) for x in words))
    print(f"wrote {out}: {len(words)} instructions, variant {variant}")


if __name__ == "__main__":
    main()
