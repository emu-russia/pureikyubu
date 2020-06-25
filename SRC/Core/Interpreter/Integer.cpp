// Integer Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    // rd = (ra | 0) + SIMM
    OP(ADDI)
    {
        if (RA) RRD = RRA + SIMM;
        else RRD = SIMM;
        Gekko->regs.pc += 4;
    }

    // rd = (ra | 0) + (SIMM || 0x0000)
    OP(ADDIS)
    {
        if (RA) RRD = RRA + (SIMM << 16);
        else RRD = SIMM << 16;
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb
    OP(ADD)
    {
        RRD = RRA + RRB;
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb, CR0
    OP(ADDD)
    {
        uint32_t res = RRA + RRB;
        RRD = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb, XER
    OP(ADDO)
    {
        uint32_t a = RRA, b = RRB, res;
        bool ovf = false;

        res = AddOverflow(a, b);
        ovf = OverflowBit != 0;

        RRD = res;
        if (ovf)
        {
            SET_XER_OV;
            SET_XER_SO;
        }
        else RESET_XER_OV;
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb, CR0, XER
    OP(ADDOD)
    {
        uint32_t a = RRA, b = RRB, res;
        bool ovf = false;

        res = AddOverflow(a, b);
        ovf = OverflowBit != 0;

        RRD = res;
        if (ovf)
        {
            SET_XER_OV;
            SET_XER_SO;
        }
        else RESET_XER_OV;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + rb + 1
    OP(SUBF)
    {
        RRD = ~RRA + RRB + 1;
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + rb + 1, CR0
    OP(SUBFD)
    {
        uint32_t res = ~RRA + RRB + 1;
        RRD = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + rb + 1, XER
    OP(SUBFO)
    {
        DBHalt("SUBFO\n");
    }

    // rd = ~ra + rb + 1, CR0, XER
    OP(SUBFOD)
    {
        DBHalt("SUBFOD\n");
    }

    // rd = ra + SIMM, XER
    OP(ADDIC)
    {
        uint32_t a = RRA, b = SIMM, res;
        bool carry = false;

        res = AddCarry(a, b);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        Gekko->regs.pc += 4;
    }

    // rd = ra + SIMM, CR0, XER
    OP(ADDICD)
    {
        uint32_t a = RRA, b = SIMM, res;
        bool carry = false;

        res = AddCarry(a, b);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // rd = ~RRA + SIMM + 1, XER
    OP(SUBFIC)
    {
        uint32_t a = ~RRA, b = SIMM + 1, res;
        bool carry = false;

        res = AddCarry(a, b);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb, XER[CA]
    OP(ADDC)
    {
        uint32_t a = RRA, b = RRB, res;
        bool carry = false;

        res = AddCarry(a, b);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb, XER[CA], CR0
    OP(ADDCD)
    {
        uint32_t a = RRA, b = RRB, res;
        bool carry = false;

        res = AddCarry(a, b);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb, XER[CA], XER[OV]
    OP(ADDCO)
    {
        uint32_t a = RRA, b = RRB, res;
        bool carry = false, ovf = false;

        res = AddCarryOverflow(a, b);
        carry = CarryBit != 0;
        ovf = OverflowBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        if (ovf)
        {
            SET_XER_OV;
            SET_XER_SO;
        }
        else RESET_XER_OV;
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb, XER[CA], XER[OV], CR0
    OP(ADDCOD)
    {
        uint32_t a = RRA, b = RRB, res;
        bool carry = false, ovf = false;

        res = AddCarryOverflow(a, b);
        carry = CarryBit != 0;
        ovf = OverflowBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        if (ovf)
        {
            SET_XER_OV;
            SET_XER_SO;
        }
        else RESET_XER_OV;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + rb + 1, XER[CA]
    OP(SUBFC)
    {
        uint32_t a = ~RRA, b = RRB + 1, res;
        bool carry = false;

        res = AddCarry(a, b);
        carry = CarryBit != 0;

        if (carry) SET_XER_CA; else RESET_XER_CA;
        RRD = res;
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + rb + 1, XER[CA], CR0
    OP(SUBFCD)
    {
        uint32_t a = ~RRA, b = RRB + 1, res;
        bool carry = false;

        res = AddCarry(a, b);
        carry = CarryBit != 0;

        if (carry) SET_XER_CA; else RESET_XER_CA;
        RRD = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ---------------------------------------------------------------------------

    static void ADDXER(uint32_t a, uint32_t op)
    {
        uint32_t res;
        uint32_t c = (IS_XER_CA) ? 1 : 0;
        bool carry = false;

        res = AddCarry(a, c);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
    }

    static void ADDXERD(uint32_t a, uint32_t op)
    {
        uint32_t res;
        uint32_t c = (IS_XER_CA) ? 1 : 0;
        bool carry = false;

        res = AddCarry(a, c);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        COMPUTE_CR0(res);
    }

    static void ADDXER2(uint32_t a, uint32_t b, uint32_t op)
    {
        uint32_t res;
        uint32_t c = (IS_XER_CA) ? 1 : 0;
        bool carry = false;

        CarryBit = c;
        res = AddXer2(a, b);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
    }

    static void ADDXER2D(uint32_t a, uint32_t b, uint32_t op)
    {
        uint32_t res;
        uint32_t c = (IS_XER_CA) ? 1 : 0;
        bool carry = false;

        CarryBit = c;
        res = AddXer2(a, b);
        carry = CarryBit != 0;

        RRD = res;
        if (carry) SET_XER_CA; else RESET_XER_CA;
        COMPUTE_CR0(res);
    }

    // rd = ra + rb + XER[CA], XER
    OP(ADDE)
    {
        ADDXER2(RRA, RRB, op);
        Gekko->regs.pc += 4;
    }

    // rd = ra + rb + XER[CA], CR0, XER
    OP(ADDED)
    {
        ADDXER2D(RRA, RRB, op);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + rb + XER[CA], XER
    OP(SUBFE)
    {
        ADDXER2(~RRA, RRB, op);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + rb + XER[CA], CR0, XER
    OP(SUBFED)
    {
        ADDXER2D(~RRA, RRB, op);
        Gekko->regs.pc += 4;
    }

    // rd = ra + XER[CA] - 1 (0xffffffff), XER
    OP(ADDME)
    {
        ADDXER(RRA - 1, op);
        Gekko->regs.pc += 4;
    }

    // rd = ra + XER[CA] - 1 (0xffffffff), CR0, XER
    OP(ADDMED)
    {
        ADDXERD(RRA - 1, op);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + XER[CA] - 1, XER
    OP(SUBFME)
    {
        ADDXER(~RRA - 1, op);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + XER[CA] - 1, CR0, XER
    OP(SUBFMED)
    {
        ADDXERD(~RRA - 1, op);
        Gekko->regs.pc += 4;
    }

    // rd = ra + XER[CA], XER
    OP(ADDZE)
    {
        ADDXER(RRA, op);
        Gekko->regs.pc += 4;
    }

    // rd = ra + XER[CA], CR0, XER
    OP(ADDZED)
    {
        ADDXERD(RRA, op);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + XER[CA], XER
    OP(SUBFZE)
    {
        ADDXER(~RRA, op);
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + XER[CA], CR0, XER
    OP(SUBFZED)
    {
        ADDXERD(~RRA, op);
        Gekko->regs.pc += 4;
    }

    // ---------------------------------------------------------------------------

    // rd = ~ra + 1
    OP(NEG)
    {
        RRD = ~RRA + 1;
        Gekko->regs.pc += 4;
    }

    // rd = ~ra + 1, CR0
    OP(NEGD)
    {
        uint32_t res = ~RRA + 1;
        RRD = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ---------------------------------------------------------------------------

    // prod[0-48] = ra * SIMM
    // rd = prod[16-48]
    OP(MULLI)
    {
        RRD = RRA * SIMM;
        Gekko->regs.pc += 4;
    }

    // prod[0-48] = ra * rb
    // rd = prod[16-48]
    OP(MULLW)
    {
        int32_t a = RRA, b = RRB;
        int64_t res = (int64_t)a * (int64_t)b;
        RRD = (int32_t)(res & 0x00000000ffffffff);
        Gekko->regs.pc += 4;
    }

    // prod[0-48] = ra * rb
    // rd = prod[16-48]
    // CR0
    OP(MULLWD)
    {
        int32_t a = RRA, b = RRB;
        int64_t res = (int64_t)a * (int64_t)b;
        RRD = (int32_t)(res & 0x00000000ffffffff);
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // prod[0-63] = ra * rb
    // rd = prod[0-31]
    OP(MULHW)
    {
        int64_t a = (int32_t)RRA, b = (int32_t)RRB, res = a * b;
        res = (res >> 32);
        RRD = (int32_t)res;
        Gekko->regs.pc += 4;
    }

    // prod[0-63] = ra * rb
    // rd = prod[0-31]
    // CR0
    OP(MULHWD)
    {
        int64_t a = (int32_t)RRA, b = (int32_t)RRB, res = a * b;
        res = (res >> 32);
        RRD = (int32_t)res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // prod[0-63] = ra * rb
    // rd = prod[0-31]
    OP(MULHWU)
    {
        uint64_t a = RRA, b = RRB, res = a * b;
        res = (res >> 32);
        RRD = (uint32_t)res;
        Gekko->regs.pc += 4;
    }

    // prod[0-63] = ra * rb
    // rd = prod[0-31]
    // CR0
    OP(MULHWUD)
    {
        uint64_t a = RRA, b = RRB, res = a * b;
        res = (res >> 32);
        RRD = (uint32_t)res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // rd = ra / rb (signed)
    OP(DIVW)
    {
        int32_t a = RRA, b = RRB;
        if (b) RRD = a / b;
        Gekko->regs.pc += 4;
    }

    // rd = ra / rb (signed), CR0
    OP(DIVWD)
    {
        int32_t a = RRA, b = RRB, res;
        if (b)
        {
            res = a / b;
            RRD = res;
            COMPUTE_CR0(res);
        }
        Gekko->regs.pc += 4;
    }

    // rd = ra / rb (unsigned)
    OP(DIVWU)
    {
        uint32_t a = RRA, b = RRB;
        if (b) RRD = a / b;
        Gekko->regs.pc += 4;
    }

    // rd = ra / rb (unsigned), CR0
    OP(DIVWUD)
    {
        uint32_t a = RRA, b = RRB, res;
        if (b)
        {
            res = a / b;
            RRD = res;
            COMPUTE_CR0(res);
        }
        Gekko->regs.pc += 4;
    }

}
