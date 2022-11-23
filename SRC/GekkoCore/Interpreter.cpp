// Gekko interpreter
#include "pch.h"

namespace Gekko
{
    Interpreter::Interpreter(GekkoCore* _core)
    {
        core = _core;

        // build rotate mask table
        for (int mb = 0; mb < 32; mb++)
        {
            for (int me = 0; me < 32; me++)
            {
                uint32_t mask = ((uint32_t)-1 >> mb) ^ ((me >= 31) ? 0 : ((uint32_t)-1) >> (me + 1));
                rotmask[mb][me] = (mb > me) ? (~mask) : (mask);
            }
        }

        // build paired-single load scale
        for (uint8_t scale = 0; scale < 64; scale++)
        {
            int factor;
            if (scale & 0x20)    // -32 ... -1
            {
                factor = -32 + (scale & 0x1f);
            }
            else                // 0 ... 31
            {
                factor = 0 + (scale & 0x1f);
            }
            ldScale[scale] = powf(2, -1.0f * (float)factor);
        }

        // build paired-single store scale
        for (uint8_t scale = 0; scale < 64; scale++)
        {
            int factor;
            if (scale & 0x20)    // -32 ... -1
            {
                factor = -32 + (scale & 0x1f);
            }
            else                // 0 ... 31
            {
                factor = 0 + (scale & 0x1f);
            }
            stScale[scale] = powf(2, +1.0f * (float)factor);
        }
    }

    /// <summary>
    /// Result = a + b + CarryIn. Return carry flag in CarryBit. Return overflow flag in OverflowBit.
    /// </summary>
    /// <param name="a"></param>
    /// <param name="b"></param>
    /// <returns></returns>
    uint32_t Interpreter::FullAdder(uint32_t a, uint32_t b)
    {
        uint64_t res = (uint64_t)a + (uint64_t)b + (uint64_t)(CarryBit != 0 ? 1 : 0);

        //A human need only remember that, when doing signed math, adding
        //two numbers of the same sign must produce a result of the same sign,
        //otherwise overflow happened.
        bool msb = (res & 0x8000'0000) != 0 ? true : false;
        bool aMsb = (a & 0x8000'0000) != 0 ? true : false;
        bool bMsb = (b & 0x8000'0000) != 0 ? true : false;
        OverflowBit = 0;
        if (aMsb == bMsb)
        {
            OverflowBit = (aMsb != msb) ? 1 : 0;
        }

        CarryBit = ((res & 0xffffffff'00000000) != 0) ? 1 : 0;

        return (uint32_t)res;
    }

    /// <summary>
    /// Rotate 32 bit left
    /// </summary>
    /// <param name="sa">Rotate bits amount</param>
    /// <param name="data">Source</param>
    /// <returns>Result</returns>
    uint32_t Interpreter::Rotl32(size_t sa, uint32_t data)
    {
        return (data << sa) | (data >> ((32 - sa) & 31));
    }


    // parse and execute single opcode
    void Interpreter::ExecuteOpcode()
    {
        int WIMG;
        uint32_t instr = 0, pa;

        // execute one instruction
        // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
        pa = core->EffectiveToPhysical(core->regs.pc, MmuAccess::Execute, WIMG);
        if (pa == Gekko::BadAddress)
        {
            core->Exception(Exception::EXCEPTION_ISI);
        }
        else
        {
            SixtyBus_ReadWord(pa, &instr);
        }
        // ISI
        if (core->exception)
        {
            core->exception = false;
            return;
        }
        
        // Decode instruction using GekkoAnalyzer and dispatch

        Decoder::Decode(core->regs.pc, instr, &info);
        Dispatch();
        core->ops++;

        if (core->resetInstructionCounter)
        {
            core->resetInstructionCounter = false;
            core->ops = 0;
        }
        // DSI, ALIGN, PROGRAM, FPUNA, SC
        if (core->exception)
        {
            core->exception = false;
            return;
        }

        core->Tick();

        core->exception = false;
    }

    /// <summary>
    /// Auxiliary call for the recompiler, to execute instructions that are not implemented there.
    /// </summary>
    /// <returns>true: An exception occurred during the execution of an instruction, so it is necessary to abort the execution of the recompiled code segment.</returns>
    bool Interpreter::ExecuteInterpeterFallback()
    {
        int WIMG;
        uint32_t instr = 0, pa;

        // execute one instruction
        // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
        pa = core->EffectiveToPhysical(core->regs.pc, MmuAccess::Execute, WIMG);
        if (pa == Gekko::BadAddress)
        {
            core->Exception(Exception::EXCEPTION_ISI);
        }
        else
        {
            SixtyBus_ReadWord(pa, &instr);
        }

        if (core->exception)
        {
            // ISI
            core->exception = false;
            return true;
        }
        
        // Decode instruction using GekkoAnalyzer and dispatch

        Decoder::DecodeFast(core->regs.pc, instr, &info);
        Dispatch();
        core->ops++;

        if (core->resetInstructionCounter)
        {
            core->resetInstructionCounter = false;
            core->ops = 0;
        }

        if (core->exception)
        {
            // DSI, ALIGN, PROGRAM, FPUNA, SC
            core->exception = false;
            return true;
        }

        core->Tick();

        return false;
    }

    void Interpreter::Dispatch()
    {
        switch (info.instr)
        {
            case Instruction::b: b(); break;
            case Instruction::ba: ba(); break;
            case Instruction::bl: bl(); break;
            case Instruction::bla: bla(); break;
            case Instruction::bc: bc(); break;
            case Instruction::bca: bca(); break;
            case Instruction::bcl: bcl(); break;
            case Instruction::bcla: bcla(); break;
            case Instruction::bcctr: bcctr(); break;
            case Instruction::bcctrl: bcctrl(); break;
            case Instruction::bclr: bclr(); break;
            case Instruction::bclrl: bclrl(); break;

            case Instruction::cmpi: cmpi(); break;
            case Instruction::cmp: cmp(); break;
            case Instruction::cmpli: cmpli(); break;
            case Instruction::cmpl: cmpl(); break;

            case Instruction::crand: crand(); break;
            case Instruction::crandc: crandc(); break;
            case Instruction::creqv: creqv(); break;
            case Instruction::crnand: crnand(); break;
            case Instruction::crnor: crnor(); break;
            case Instruction::cror: cror(); break;
            case Instruction::crorc: crorc(); break;
            case Instruction::crxor: crxor(); break;
            case Instruction::mcrf: mcrf(); break;

            case Instruction::fadd: fadd(); break;
            case Instruction::fadd_d: fadd_d(); break;
            case Instruction::fadds: fadds(); break;
            case Instruction::fadds_d: fadds_d(); break;
            case Instruction::fdiv: fdiv(); break;
            case Instruction::fdiv_d: fdiv_d(); break;
            case Instruction::fdivs: fdivs(); break;
            case Instruction::fdivs_d: fdivs_d(); break;
            case Instruction::fmul: fmul(); break;
            case Instruction::fmul_d: fmul_d(); break;
            case Instruction::fmuls: fmuls(); break;
            case Instruction::fmuls_d: fmuls_d(); break;
            case Instruction::fres: fres(); break;
            case Instruction::fres_d: fres_d(); break;
            case Instruction::frsqrte: frsqrte(); break;
            case Instruction::frsqrte_d: frsqrte_d(); break;
            case Instruction::fsub: fsub(); break;
            case Instruction::fsub_d: fsub_d(); break;
            case Instruction::fsubs: fsubs(); break;
            case Instruction::fsubs_d: fsubs_d(); break;
            case Instruction::fsel: fsel(); break;
            case Instruction::fsel_d: fsel_d(); break;
            case Instruction::fmadd: fmadd(); break;
            case Instruction::fmadd_d: fmadd_d(); break;
            case Instruction::fmadds: fmadds(); break;
            case Instruction::fmadds_d: fmadds_d(); break;
            case Instruction::fmsub: fmsub(); break;
            case Instruction::fmsub_d: fmsub_d(); break;
            case Instruction::fmsubs: fmsubs(); break;
            case Instruction::fmsubs_d: fmsubs_d(); break;
            case Instruction::fnmadd: fnmadd(); break;
            case Instruction::fnmadd_d: fnmadd_d(); break;
            case Instruction::fnmadds: fnmadds(); break;
            case Instruction::fnmadds_d: fnmadds_d(); break;
            case Instruction::fnmsub: fnmsub(); break;
            case Instruction::fnmsub_d: fnmsub_d(); break;
            case Instruction::fnmsubs: fnmsubs(); break;
            case Instruction::fnmsubs_d: fnmsubs_d(); break;
            case Instruction::fctiw: fctiw(); break;
            case Instruction::fctiw_d: fctiw_d(); break;
            case Instruction::fctiwz: fctiwz(); break;
            case Instruction::fctiwz_d: fctiwz_d(); break;
            case Instruction::frsp: frsp(); break;
            case Instruction::frsp_d: frsp_d(); break;
            case Instruction::fcmpo: fcmpo(); break;
            case Instruction::fcmpu: fcmpu(); break;
            case Instruction::fabs: fabs(); break;
            case Instruction::fabs_d: fabs_d(); break;
            case Instruction::fmr: fmr(); break;
            case Instruction::fmr_d: fmr_d(); break;
            case Instruction::fnabs: fnabs(); break;
            case Instruction::fnabs_d: fnabs_d(); break;
            case Instruction::fneg: fneg(); break;
            case Instruction::fneg_d: fneg_d(); break;

            case Instruction::mcrfs: mcrfs(); break;
            case Instruction::mffs: mffs(); break;
            case Instruction::mffs_d: mffs_d(); break;
            case Instruction::mtfsb0: mtfsb0(); break;
            case Instruction::mtfsb0_d: mtfsb0_d(); break;
            case Instruction::mtfsb1: mtfsb1(); break;
            case Instruction::mtfsb1_d: mtfsb1_d(); break;
            case Instruction::mtfsf: mtfsf(); break;
            case Instruction::mtfsf_d: mtfsf_d(); break;
            case Instruction::mtfsfi: mtfsfi(); break;
            case Instruction::mtfsfi_d: mtfsfi_d(); break;

            case Instruction::lfd: lfd(); break;
            case Instruction::lfdu: lfdu(); break;
            case Instruction::lfdux: lfdux(); break;
            case Instruction::lfdx: lfdx(); break;
            case Instruction::lfs: lfs(); break;
            case Instruction::lfsu: lfsu(); break;
            case Instruction::lfsux: lfsux(); break;
            case Instruction::lfsx: lfsx(); break;
            case Instruction::stfd: stfd(); break;
            case Instruction::stfdu: stfdu(); break;
            case Instruction::stfdux: stfdux(); break;
            case Instruction::stfdx: stfdx(); break;
            case Instruction::stfiwx: stfiwx(); break;
            case Instruction::stfs: stfs(); break;
            case Instruction::stfsu: stfsu(); break;
            case Instruction::stfsux: stfsux(); break;
            case Instruction::stfsx: stfsx(); break;

            case Instruction::add: add(); break;
            case Instruction::add_d: add_d(); break;
            case Instruction::addo: addo(); break;
            case Instruction::addo_d: addo_d(); break;
            case Instruction::addc: addc(); break;
            case Instruction::addc_d: addc_d(); break;
            case Instruction::addco: addco(); break;
            case Instruction::addco_d: addco_d(); break;
            case Instruction::adde: adde(); break;
            case Instruction::adde_d: adde_d(); break;
            case Instruction::addeo: addeo(); break;
            case Instruction::addeo_d: addeo_d(); break;
            case Instruction::addi: addi(); break;
            case Instruction::addic: addic(); break;
            case Instruction::addic_d: addic_d(); break;
            case Instruction::addis: addis(); break;
            case Instruction::addme: addme(); break;
            case Instruction::addme_d: addme_d(); break;
            case Instruction::addmeo: addmeo(); break;
            case Instruction::addmeo_d: addmeo_d(); break;
            case Instruction::addze: addze(); break;
            case Instruction::addze_d: addze_d(); break;
            case Instruction::addzeo: addzeo(); break;
            case Instruction::addzeo_d: addzeo_d(); break;
            case Instruction::divw: divw(); break;
            case Instruction::divw_d: divw_d(); break;
            case Instruction::divwo: divwo(); break;
            case Instruction::divwo_d: divwo_d(); break;
            case Instruction::divwu: divwu(); break;
            case Instruction::divwu_d: divwu_d(); break;
            case Instruction::divwuo: divwuo(); break;
            case Instruction::divwuo_d: divwuo_d(); break;
            case Instruction::mulhw: mulhw(); break;
            case Instruction::mulhw_d: mulhw_d(); break;
            case Instruction::mulhwu: mulhwu(); break;
            case Instruction::mulhwu_d: mulhwu_d(); break;
            case Instruction::mulli: mulli(); break;
            case Instruction::mullw: mullw(); break;
            case Instruction::mullw_d: mullw_d(); break;
            case Instruction::mullwo: mullwo(); break;
            case Instruction::mullwo_d: mullwo_d(); break;
            case Instruction::neg: neg(); break;
            case Instruction::neg_d: neg_d(); break;
            case Instruction::nego: nego(); break;
            case Instruction::nego_d: nego_d(); break;
            case Instruction::subf: subf(); break;
            case Instruction::subf_d: subf_d(); break;
            case Instruction::subfo: subfo(); break;
            case Instruction::subfo_d: subfo_d(); break;
            case Instruction::subfc: subfc(); break;
            case Instruction::subfc_d: subfc_d(); break;
            case Instruction::subfco: subfco(); break;
            case Instruction::subfco_d: subfco_d(); break;
            case Instruction::subfe: subfe(); break;
            case Instruction::subfe_d: subfe_d(); break;
            case Instruction::subfeo: subfeo(); break;
            case Instruction::subfeo_d: subfeo_d(); break;
            case Instruction::subfic: subfic(); break;
            case Instruction::subfme: subfme(); break;
            case Instruction::subfme_d: subfme_d(); break;
            case Instruction::subfmeo: subfmeo(); break;
            case Instruction::subfmeo_d: subfmeo_d(); break;
            case Instruction::subfze: subfze(); break;
            case Instruction::subfze_d: subfze_d(); break;
            case Instruction::subfzeo: subfzeo(); break;
            case Instruction::subfzeo_d: subfzeo_d(); break;

            case Instruction::lbz: lbz(); break;
            case Instruction::lbzu: lbzu(); break;
            case Instruction::lbzux: lbzux(); break;
            case Instruction::lbzx: lbzx(); break;
            case Instruction::lha: lha(); break;
            case Instruction::lhau: lhau(); break;
            case Instruction::lhaux: lhaux(); break;
            case Instruction::lhax: lhax(); break;
            case Instruction::lhz: lhz(); break;
            case Instruction::lhzu: lhzu(); break;
            case Instruction::lhzux: lhzux(); break;
            case Instruction::lhzx: lhzx(); break;
            case Instruction::lwz: lwz(); break;
            case Instruction::lwzu: lwzu(); break;
            case Instruction::lwzux: lwzux(); break;
            case Instruction::lwzx: lwzx(); break;
            case Instruction::stb: stb(); break;
            case Instruction::stbu: stbu(); break;
            case Instruction::stbux: stbux(); break;
            case Instruction::stbx: stbx(); break;
            case Instruction::sth: sth(); break;
            case Instruction::sthu: sthu(); break;
            case Instruction::sthux: sthux(); break;
            case Instruction::sthx: sthx(); break;
            case Instruction::stw: stw(); break;
            case Instruction::stwu: stwu(); break;
            case Instruction::stwux: stwux(); break;
            case Instruction::stwx: stwx(); break;
            case Instruction::lhbrx: lhbrx(); break;
            case Instruction::lwbrx: lwbrx(); break;
            case Instruction::sthbrx: sthbrx(); break;
            case Instruction::stwbrx: stwbrx(); break;
            case Instruction::lmw: lmw(); break;
            case Instruction::stmw: stmw(); break;
            case Instruction::lswi: lswi(); break;
            case Instruction::lswx: lswx(); break;
            case Instruction::stswi: stswi(); break;
            case Instruction::stswx: stswx(); break;

            case Instruction::_and: _and(); break;
            case Instruction::and_d: and_d(); break;
            case Instruction::andc: andc(); break;
            case Instruction::andc_d: andc_d(); break;
            case Instruction::andi_d: andi_d(); break;
            case Instruction::andis_d: andis_d(); break;
            case Instruction::cntlzw: cntlzw(); break;
            case Instruction::cntlzw_d: cntlzw_d(); break;
            case Instruction::eqv: eqv(); break;
            case Instruction::eqv_d: eqv_d(); break;
            case Instruction::extsb: extsb(); break;
            case Instruction::extsb_d: extsb_d(); break;
            case Instruction::extsh: extsh(); break;
            case Instruction::extsh_d: extsh_d(); break;
            case Instruction::nand: nand(); break;
            case Instruction::nand_d: nand_d(); break;
            case Instruction::nor: nor(); break;
            case Instruction::nor_d: nor_d(); break;
            case Instruction::_or: _or(); break;
            case Instruction::or_d: or_d(); break;
            case Instruction::orc: orc(); break;
            case Instruction::orc_d: orc_d(); break;
            case Instruction::ori: ori(); break;
            case Instruction::oris: oris(); break;
            case Instruction::_xor: _xor(); break;
            case Instruction::xor_d: xor_d(); break;
            case Instruction::xori: xori(); break;
            case Instruction::xoris: xoris(); break;

            case Instruction::ps_div: ps_div(); break;
            case Instruction::ps_div_d: ps_div_d(); break;
            case Instruction::ps_sub: ps_sub(); break;
            case Instruction::ps_sub_d: ps_sub_d(); break;
            case Instruction::ps_add: ps_add(); break;
            case Instruction::ps_add_d: ps_add_d(); break;
            case Instruction::ps_sel: ps_sel(); break;
            case Instruction::ps_sel_d: ps_sel_d(); break;
            case Instruction::ps_res: ps_res(); break;
            case Instruction::ps_res_d: ps_res_d(); break;
            case Instruction::ps_mul: ps_mul(); break;
            case Instruction::ps_mul_d: ps_mul_d(); break;
            case Instruction::ps_rsqrte: ps_rsqrte(); break;
            case Instruction::ps_rsqrte_d: ps_rsqrte_d(); break;
            case Instruction::ps_msub: ps_msub(); break;
            case Instruction::ps_msub_d: ps_msub_d(); break;
            case Instruction::ps_madd: ps_madd(); break;
            case Instruction::ps_madd_d: ps_madd_d(); break;
            case Instruction::ps_nmsub: ps_nmsub(); break;
            case Instruction::ps_nmsub_d: ps_nmsub_d(); break;
            case Instruction::ps_nmadd: ps_nmadd(); break;
            case Instruction::ps_nmadd_d: ps_nmadd_d(); break;
            case Instruction::ps_neg: ps_neg(); break;
            case Instruction::ps_neg_d: ps_neg_d(); break;
            case Instruction::ps_mr: ps_mr(); break;
            case Instruction::ps_mr_d: ps_mr_d(); break;
            case Instruction::ps_nabs: ps_nabs(); break;
            case Instruction::ps_nabs_d: ps_nabs_d(); break;
            case Instruction::ps_abs: ps_abs(); break;
            case Instruction::ps_abs_d: ps_abs_d(); break;

            case Instruction::ps_sum0: ps_sum0(); break;
            case Instruction::ps_sum0_d: ps_sum0_d(); break;
            case Instruction::ps_sum1: ps_sum1(); break;
            case Instruction::ps_sum1_d: ps_sum1_d(); break;
            case Instruction::ps_muls0: ps_muls0(); break;
            case Instruction::ps_muls0_d: ps_muls0_d(); break;
            case Instruction::ps_muls1: ps_muls1(); break;
            case Instruction::ps_muls1_d: ps_muls1_d(); break;
            case Instruction::ps_madds0: ps_madds0(); break;
            case Instruction::ps_madds0_d: ps_madds0_d(); break;
            case Instruction::ps_madds1: ps_madds1(); break;
            case Instruction::ps_madds1_d: ps_madds1_d(); break;
            case Instruction::ps_cmpu0: ps_cmpu0(); break;
            case Instruction::ps_cmpo0: ps_cmpo0(); break;
            case Instruction::ps_cmpu1: ps_cmpu1(); break;
            case Instruction::ps_cmpo1: ps_cmpo1(); break;
            case Instruction::ps_merge00: ps_merge00(); break;
            case Instruction::ps_merge00_d: ps_merge00_d(); break;
            case Instruction::ps_merge01: ps_merge01(); break;
            case Instruction::ps_merge01_d: ps_merge01_d(); break;
            case Instruction::ps_merge10: ps_merge10(); break;
            case Instruction::ps_merge10_d: ps_merge10_d(); break;
            case Instruction::ps_merge11: ps_merge11(); break;
            case Instruction::ps_merge11_d: ps_merge11_d(); break;

            case Instruction::psq_lx: psq_lx(); break;
            case Instruction::psq_stx: psq_stx(); break;
            case Instruction::psq_lux: psq_lux(); break;
            case Instruction::psq_stux: psq_stux(); break;
            case Instruction::psq_l: psq_l(); break;
            case Instruction::psq_lu: psq_lu(); break;
            case Instruction::psq_st: psq_st(); break;
            case Instruction::psq_stu: psq_stu(); break;

            case Instruction::rlwimi: rlwimi(); break;
            case Instruction::rlwimi_d: rlwimi_d(); break;
            case Instruction::rlwinm: rlwinm(); break;
            case Instruction::rlwinm_d: rlwinm_d(); break;
            case Instruction::rlwnm: rlwnm(); break;
            case Instruction::rlwnm_d: rlwnm_d(); break;

            case Instruction::slw: slw(); break;
            case Instruction::slw_d: slw_d(); break;
            case Instruction::sraw: sraw(); break;
            case Instruction::sraw_d: sraw_d(); break;
            case Instruction::srawi: srawi(); break;
            case Instruction::srawi_d: srawi_d(); break;
            case Instruction::srw: srw(); break;
            case Instruction::srw_d: srw_d(); break;

            case Instruction::eieio: eieio(); break;
            case Instruction::isync: isync(); break;
            case Instruction::lwarx: lwarx(); break;
            case Instruction::stwcx_d: stwcx_d(); break;
            case Instruction::sync: sync(); break;
            case Instruction::rfi: rfi(); break;
            case Instruction::sc: sc(); break;
            case Instruction::tw: tw(); break;
            case Instruction::twi: twi(); break;
            case Instruction::mcrxr: mcrxr(); break;
            case Instruction::mfcr: mfcr(); break;
            case Instruction::mfmsr: mfmsr(); break;
            case Instruction::mfspr: mfspr(); break;
            case Instruction::mftb: mftb(); break;
            case Instruction::mtcrf: mtcrf(); break;
            case Instruction::mtmsr: mtmsr(); break;
            case Instruction::mtspr: mtspr(); break;
            case Instruction::dcbf: dcbf(); break;
            case Instruction::dcbi: dcbi(); break;
            case Instruction::dcbst: dcbst(); break;
            case Instruction::dcbt: dcbt(); break;
            case Instruction::dcbtst: dcbtst(); break;
            case Instruction::dcbz: dcbz(); break;
            case Instruction::dcbz_l: dcbz_l(); break;
            case Instruction::icbi: icbi(); break;
            case Instruction::mfsr: mfsr(); break;
            case Instruction::mfsrin: mfsrin(); break;
            case Instruction::mtsr: mtsr(); break;
            case Instruction::mtsrin: mtsrin(); break;
            case Instruction::tlbie: tlbie(); break;
            case Instruction::tlbsync: tlbsync(); break;
            case Instruction::eciwx: eciwx(); break;
            case Instruction::ecowx: ecowx(); break;

            // TODO: CallVM opcode.

            default:
                core->Halt("** CPU ERROR **\n"
                    "unimplemented opcode : %08X\n", core->regs.pc);

                core->PrCause = PrivilegedCause::IllegalInstruction;
                core->Exception(Exception::EXCEPTION_PROGRAM);
                return;
        }

        if (core->opcodeStatsEnabled)
        {
            core->opcodeStats[(size_t)info.instr]++;
        }
    }

    uint32_t Interpreter::GetRotMask(int mb, int me)
    {
        return rotmask[mb][me];
    }

    // high level call
    void Interpreter::callvm()
    {
        // Dolwin module base should be specified as 0x400000 in project properties
        //void (*pcall)() = (void (*)())((void*)(uint64_t)op);

        //if (op == 0)
        //{
        //	Halt(
        //		"Something goes wrong in interpreter, \n"
        //		"program is trying to execute NULL opcode.\n\n"
        //		"pc:%08X", core->regs.pc);
        //	return;
        //}

        //pcall();

        core->Halt("callvm: Temporary not implemented!\n");
    }

}
