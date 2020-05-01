/// Private interpreter stuff

#pragma once

#define OP(name) void Interpreter::c_##name##(uint32_t op)

#define COMPUTE_CR0(r)                                                                \
{                                                                                     \
    (Gekko::Gekko->regs.cr = (Gekko::Gekko->regs.cr & 0x0fff'ffff)                   |  \
    ((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & (1 << 31)) ? (0x1000'0000) : (0)) |    \
    (((int32_t)(r) < 0) ? (0x8000'0000) : (((int32_t)(r) > 0) ? (0x4000'0000) : (0x2000'0000))));\
}

#define COMPUTE_CR1()                                                                 \
{                                                                                     \
    Gekko::Gekko->regs.cr = (Gekko::Gekko->regs.cr & 0xf0ff'ffff) | ((Gekko::Gekko->regs.fpscr & 0xf000'0000) >> 4);    \
}

#define SET_CR_LT(n)    (Gekko::Gekko->regs.cr |=  (1 << (3 + 4 * (7 - n))))
#define SET_CR_GT(n)    (Gekko::Gekko->regs.cr |=  (1 << (2 + 4 * (7 - n))))
#define SET_CR_EQ(n)    (Gekko::Gekko->regs.cr |=  (1 << (1 + 4 * (7 - n))))
#define SET_CR_SO(n)    (Gekko::Gekko->regs.cr |=  (1 << (    4 * (7 - n))))
#define RESET_CR_LT(n)  (Gekko::Gekko->regs.cr &= ~(1 << (3 + 4 * (7 - n))))
#define RESET_CR_GT(n)  (Gekko::Gekko->regs.cr &= ~(1 << (2 + 4 * (7 - n))))
#define RESET_CR_EQ(n)  (Gekko::Gekko->regs.cr &= ~(1 << (1 + 4 * (7 - n))))
#define RESET_CR_SO(n)  (Gekko::Gekko->regs.cr &= ~(1 << (    4 * (7 - n))))

#define SET_CR0_LT      (Gekko::Gekko->regs.cr |=  (1 << 31))
#define SET_CR0_GT      (Gekko::Gekko->regs.cr |=  (1 << 30))
#define SET_CR0_EQ      (Gekko::Gekko->regs.cr |=  (1 << 29))
#define SET_CR0_SO      (Gekko::Gekko->regs.cr |=  (1 << 28))
#define RESET_CR0_LT    (Gekko::Gekko->regs.cr &= ~(1 << 31))
#define RESET_CR0_GT    (Gekko::Gekko->regs.cr &= ~(1 << 30))
#define RESET_CR0_EQ    (Gekko::Gekko->regs.cr &= ~(1 << 29))
#define RESET_CR0_SO    (Gekko::Gekko->regs.cr &= ~(1 << 28))

#define SET_XER_SO      (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] |=  (1 << 31))
#define SET_XER_OV      (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] |=  (1 << 30))
#define SET_XER_CA      (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] |=  (1 << 29))

#define RESET_XER_SO    (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~(1 << 31))
#define RESET_XER_OV    (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~(1 << 30))
#define RESET_XER_CA    (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~(1 << 29))

#define IS_XER_SO       (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & (1 << 31))
#define IS_XER_OV       (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & (1 << 30))
#define IS_XER_CA       (Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & (1 << 29))

#define IS_NAN(n)       (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0)
#define IS_SNAN(n)      (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0 && ((n) & 0x0008000000000000) == 0)
#define SET_CRF(n, c)   (Gekko::Gekko->regs.cr = (Gekko::Gekko->regs.cr & (~(0xf0000000 >> (4 * n)))) | (c << (4 * (7 - n))))

extern "C" uint32_t CarryBit;
extern "C" uint32_t OverflowBit;

extern "C" uint32_t __fastcall AddCarry(uint32_t a, uint32_t b);
extern "C" uint32_t __fastcall AddOverflow(uint32_t a, uint32_t b);
extern "C" uint32_t __fastcall AddCarryOverflow(uint32_t a, uint32_t b);
extern "C" uint32_t __fastcall AddXer2(uint32_t a, uint32_t b);
extern "C" uint32_t __fastcall Rotl32(int sa, uint32_t data);

// ---------------------------------------------------------------------------
// opcode decoding ("op" representing current opcode, to simplify macros)

#define RD          ((op >> 21) & 0x1f)
#define RS          RD
#define RA          ((op >> 16) & 0x1f)
#define RB          ((op >> 11) & 0x1f)
#define RC          ((op >>  6) & 0x1f)
#define SIMM        ((int32_t)(int16_t)(uint16_t)op)
#define UIMM        (op & 0xffff)
#define CRFD        ((op >> 23) & 7)
#define CRFS        ((op >> 18) & 7)
#define CRBD        ((op >> 21) & 0x1f)
#define CRBA        ((op >> 16) & 0x1f)
#define CRBB        ((op >> 11) & 0x1f)
#define BO(n)       ((bo >> (4-n)) & 1)
#define BI          RA
#define SH          RB
#define MB          ((op >> 6) & 0x1f)
#define ME          ((op >> 1) & 0x1f)
#define CRM         ((op >> 12) & 0xff)
#define FM          ((op >> 17) & 0xff)

// fast R*-field register addressing
#define RRD         Gekko::Gekko->regs.gpr[RD]
#define RRS         Gekko::Gekko->regs.gpr[RS]
#define RRA         Gekko::Gekko->regs.gpr[RA]
#define RRB         Gekko::Gekko->regs.gpr[RB]
#define RRC         Gekko::Gekko->regs.gpr[RC]

#define FPRU(n) (Gekko->regs.fpr[n].uval)
#define FPRD(n) (Gekko->regs.fpr[n].dbl)
#define PS0(n)  (Gekko->regs.fpr[n].dbl)
#define PS1(n)  (Gekko->regs.ps1[n].dbl)
