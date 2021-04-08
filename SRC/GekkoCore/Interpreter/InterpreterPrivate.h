// Private interpreter stuff

#pragma once

#define COMPUTE_CR0(r)                                                                \
{                                                                                     \
    (core->regs.cr = (core->regs.cr & 0x0fff'ffff)                   |  \
    ((core->regs.spr[(int)Gekko::SPR::XER] & (1 << 31)) ? (0x1000'0000) : (0)) |    \
    (((int32_t)(r) < 0) ? (0x8000'0000) : (((int32_t)(r) > 0) ? (0x4000'0000) : (0x2000'0000))));\
}

#define COMPUTE_CR1()                                                                 \
{                                                                                     \
    core->regs.cr = (core->regs.cr & 0xf0ff'ffff) | ((core->regs.fpscr & 0xf000'0000) >> 4);    \
}

#define SET_CR0_LT      (core->regs.cr |=  (1 << 31))
#define SET_CR0_GT      (core->regs.cr |=  (1 << 30))
#define SET_CR0_EQ      (core->regs.cr |=  (1 << 29))
#define SET_CR0_SO      (core->regs.cr |=  (1 << 28))
#define RESET_CR0_LT    (core->regs.cr &= ~(1 << 31))
#define RESET_CR0_GT    (core->regs.cr &= ~(1 << 30))
#define RESET_CR0_EQ    (core->regs.cr &= ~(1 << 29))
#define RESET_CR0_SO    (core->regs.cr &= ~(1 << 28))

#define SET_XER_SO      (core->regs.spr[(int)Gekko::SPR::XER] |=  (1 << 31))
#define SET_XER_OV      (core->regs.spr[(int)Gekko::SPR::XER] |=  (1 << 30))
#define SET_XER_CA      (core->regs.spr[(int)Gekko::SPR::XER] |=  (1 << 29))

#define RESET_XER_SO    (core->regs.spr[(int)Gekko::SPR::XER] &= ~(1 << 31))
#define RESET_XER_OV    (core->regs.spr[(int)Gekko::SPR::XER] &= ~(1 << 30))
#define RESET_XER_CA    (core->regs.spr[(int)Gekko::SPR::XER] &= ~(1 << 29))

#define IS_XER_SO       (core->regs.spr[(int)Gekko::SPR::XER] & (1 << 31))
#define IS_XER_OV       (core->regs.spr[(int)Gekko::SPR::XER] & (1 << 30))
#define IS_XER_CA       (core->regs.spr[(int)Gekko::SPR::XER] & (1 << 29))

#define IS_NAN(n)       (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0)
#define IS_SNAN(n)      (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0 && ((n) & 0x0008000000000000) == 0)
#define SET_CRF(n, c)   (core->regs.cr = (core->regs.cr & (~(0xf0000000 >> (4 * n)))) | (c << (4 * (7 - n))))

extern "C" uint32_t CarryBit;
extern "C" uint32_t OverflowBit;

extern "C" uint32_t __FASTCALL AddCarry(uint32_t a, uint32_t b);
extern "C" uint32_t __FASTCALL AddOverflow(uint32_t a, uint32_t b);
extern "C" uint32_t __FASTCALL AddCarryOverflow(uint32_t a, uint32_t b);
extern "C" uint32_t __FASTCALL AddXer2(uint32_t a, uint32_t b);
extern "C" uint32_t __FASTCALL Rotl32(int sa, uint32_t data);

#define FPRU(n) (core->regs.fpr[n].uval)
#define FPRD(n) (core->regs.fpr[n].dbl)
#define PS0(n)  (core->regs.fpr[n].dbl)
#define PS1(n)  (core->regs.ps1[n].dbl)
