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
            core->Exception(Exception::ISI);
        }
        else
        {
            PIReadWord(pa, &instr);
        }
        // ISI
        if (core->exception)
        {
            core->exception = false;
            return;
        }
        
        // Decode instruction using GekkoAnalyzer and dispatch

        AnalyzeInfo info;
        Analyzer::AnalyzeFast(core->regs.pc, instr, &info);
        Dispatch(info);
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

    // For testing
    void Interpreter::ExecuteOpcodeDirect(uint32_t pc, uint32_t instr)
    {
        core->regs.pc = pc;
        AnalyzeInfo info;
        Analyzer::AnalyzeFast(core->regs.pc, instr, &info);
        Dispatch(info);
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
            core->Exception(Exception::ISI);
        }
        else
        {
            PIReadWord(pa, &instr);
        }

        if (core->exception)
        {
            // ISI
            core->exception = false;
            return true;
        }
        
        // Decode instruction using GekkoAnalyzer and dispatch

        AnalyzeInfo info;
        Analyzer::AnalyzeFast(core->regs.pc, instr, &info);
        Dispatch(info);
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

    void Interpreter::Dispatch(AnalyzeInfo& info)
    {
        switch (info.instr)
        {
            case Instruction::b: b(info); break;
            case Instruction::ba: ba(info); break;
            case Instruction::bl: bl(info); break;
            case Instruction::bla: bla(info); break;
            case Instruction::bc: bc(info); break;
            case Instruction::bca: bca(info); break;
            case Instruction::bcl: bcl(info); break;
            case Instruction::bcla: bcla(info); break;
            case Instruction::bcctr: bcctr(info); break;
            case Instruction::bcctrl: bcctrl(info); break;
            case Instruction::bclr: bclr(info); break;
            case Instruction::bclrl: bclrl(info); break;

            case Instruction::cmpi: cmpi(info); break;
            case Instruction::cmp: cmp(info); break;
            case Instruction::cmpli: cmpli(info); break;
            case Instruction::cmpl: cmpl(info); break;

            case Instruction::crand: crand(info); break;
            case Instruction::crandc: crandc(info); break;
            case Instruction::creqv: creqv(info); break;
            case Instruction::crnand: crnand(info); break;
            case Instruction::crnor: crnor(info); break;
            case Instruction::cror: cror(info); break;
            case Instruction::crorc: crorc(info); break;
            case Instruction::crxor: crxor(info); break;
            case Instruction::mcrf: mcrf(info); break;

            case Instruction::fadd: fadd(info); break;
            case Instruction::fadd_d: fadd_d(info); break;
            case Instruction::fadds: fadds(info); break;
            case Instruction::fadds_d: fadds_d(info); break;
            case Instruction::fdiv: fdiv(info); break;
            case Instruction::fdiv_d: fdiv_d(info); break;
            case Instruction::fdivs: fdivs(info); break;
            case Instruction::fdivs_d: fdivs_d(info); break;
            case Instruction::fmul: fmul(info); break;
            case Instruction::fmul_d: fmul_d(info); break;
            case Instruction::fmuls: fmuls(info); break;
            case Instruction::fmuls_d: fmuls_d(info); break;
            case Instruction::fres: fres(info); break;
            case Instruction::fres_d: fres_d(info); break;
            case Instruction::frsqrte: frsqrte(info); break;
            case Instruction::frsqrte_d: frsqrte_d(info); break;
            case Instruction::fsub: fsub(info); break;
            case Instruction::fsub_d: fsub_d(info); break;
            case Instruction::fsubs: fsubs(info); break;
            case Instruction::fsubs_d: fsubs_d(info); break;
            case Instruction::fsel: fsel(info); break;
            case Instruction::fsel_d: fsel_d(info); break;
            case Instruction::fmadd: fmadd(info); break;
            case Instruction::fmadd_d: fmadd_d(info); break;
            case Instruction::fmadds: fmadds(info); break;
            case Instruction::fmadds_d: fmadds_d(info); break;
            case Instruction::fmsub: fmsub(info); break;
            case Instruction::fmsub_d: fmsub_d(info); break;
            case Instruction::fmsubs: fmsubs(info); break;
            case Instruction::fmsubs_d: fmsubs_d(info); break;
            case Instruction::fnmadd: fnmadd(info); break;
            case Instruction::fnmadd_d: fnmadd_d(info); break;
            case Instruction::fnmadds: fnmadds(info); break;
            case Instruction::fnmadds_d: fnmadds_d(info); break;
            case Instruction::fnmsub: fnmsub(info); break;
            case Instruction::fnmsub_d: fnmsub_d(info); break;
            case Instruction::fnmsubs: fnmsubs(info); break;
            case Instruction::fnmsubs_d: fnmsubs_d(info); break;
            case Instruction::fctiw: fctiw(info); break;
            case Instruction::fctiw_d: fctiw_d(info); break;
            case Instruction::fctiwz: fctiwz(info); break;
            case Instruction::fctiwz_d: fctiwz_d(info); break;
            case Instruction::frsp: frsp(info); break;
            case Instruction::frsp_d: frsp_d(info); break;
            case Instruction::fcmpo: fcmpo(info); break;
            case Instruction::fcmpu: fcmpu(info); break;
            case Instruction::fabs: fabs(info); break;
            case Instruction::fabs_d: fabs_d(info); break;
            case Instruction::fmr: fmr(info); break;
            case Instruction::fmr_d: fmr_d(info); break;
            case Instruction::fnabs: fnabs(info); break;
            case Instruction::fnabs_d: fnabs_d(info); break;
            case Instruction::fneg: fneg(info); break;
            case Instruction::fneg_d: fneg_d(info); break;

            case Instruction::mcrfs: mcrfs(info); break;
            case Instruction::mffs: mffs(info); break;
            case Instruction::mffs_d: mffs_d(info); break;
            case Instruction::mtfsb0: mtfsb0(info); break;
            case Instruction::mtfsb0_d: mtfsb0_d(info); break;
            case Instruction::mtfsb1: mtfsb1(info); break;
            case Instruction::mtfsb1_d: mtfsb1_d(info); break;
            case Instruction::mtfsf: mtfsf(info); break;
            case Instruction::mtfsf_d: mtfsf_d(info); break;
            case Instruction::mtfsfi: mtfsfi(info); break;
            case Instruction::mtfsfi_d: mtfsfi_d(info); break;

            case Instruction::lfd: lfd(info); break;
            case Instruction::lfdu: lfdu(info); break;
            case Instruction::lfdux: lfdux(info); break;
            case Instruction::lfdx: lfdx(info); break;
            case Instruction::lfs: lfs(info); break;
            case Instruction::lfsu: lfsu(info); break;
            case Instruction::lfsux: lfsux(info); break;
            case Instruction::lfsx: lfsx(info); break;
            case Instruction::stfd: stfd(info); break;
            case Instruction::stfdu: stfdu(info); break;
            case Instruction::stfdux: stfdux(info); break;
            case Instruction::stfdx: stfdx(info); break;
            case Instruction::stfiwx: stfiwx(info); break;
            case Instruction::stfs: stfs(info); break;
            case Instruction::stfsu: stfsu(info); break;
            case Instruction::stfsux: stfsux(info); break;
            case Instruction::stfsx: stfsx(info); break;

            case Instruction::add: add(info); break;
            case Instruction::add_d: add_d(info); break;
            case Instruction::addo: addo(info); break;
            case Instruction::addo_d: addo_d(info); break;
            case Instruction::addc: addc(info); break;
            case Instruction::addc_d: addc_d(info); break;
            case Instruction::addco: addco(info); break;
            case Instruction::addco_d: addco_d(info); break;
            case Instruction::adde: adde(info); break;
            case Instruction::adde_d: adde_d(info); break;
            case Instruction::addeo: addeo(info); break;
            case Instruction::addeo_d: addeo_d(info); break;
            case Instruction::addi: addi(info); break;
            case Instruction::addic: addic(info); break;
            case Instruction::addic_d: addic_d(info); break;
            case Instruction::addis: addis(info); break;
            case Instruction::addme: addme(info); break;
            case Instruction::addme_d: addme_d(info); break;
            case Instruction::addmeo: addmeo(info); break;
            case Instruction::addmeo_d: addmeo_d(info); break;
            case Instruction::addze: addze(info); break;
            case Instruction::addze_d: addze_d(info); break;
            case Instruction::addzeo: addzeo(info); break;
            case Instruction::addzeo_d: addzeo_d(info); break;
            case Instruction::divw: divw(info); break;
            case Instruction::divw_d: divw_d(info); break;
            case Instruction::divwo: divwo(info); break;
            case Instruction::divwo_d: divwo_d(info); break;
            case Instruction::divwu: divwu(info); break;
            case Instruction::divwu_d: divwu_d(info); break;
            case Instruction::divwuo: divwuo(info); break;
            case Instruction::divwuo_d: divwuo_d(info); break;
            case Instruction::mulhw: mulhw(info); break;
            case Instruction::mulhw_d: mulhw_d(info); break;
            case Instruction::mulhwu: mulhwu(info); break;
            case Instruction::mulhwu_d: mulhwu_d(info); break;
            case Instruction::mulli: mulli(info); break;
            case Instruction::mullw: mullw(info); break;
            case Instruction::mullw_d: mullw_d(info); break;
            case Instruction::mullwo: mullwo(info); break;
            case Instruction::mullwo_d: mullwo_d(info); break;
            case Instruction::neg: neg(info); break;
            case Instruction::neg_d: neg_d(info); break;
            case Instruction::nego: nego(info); break;
            case Instruction::nego_d: nego_d(info); break;
            case Instruction::subf: subf(info); break;
            case Instruction::subf_d: subf_d(info); break;
            case Instruction::subfo: subfo(info); break;
            case Instruction::subfo_d: subfo_d(info); break;
            case Instruction::subfc: subfc(info); break;
            case Instruction::subfc_d: subfc_d(info); break;
            case Instruction::subfco: subfco(info); break;
            case Instruction::subfco_d: subfco_d(info); break;
            case Instruction::subfe: subfe(info); break;
            case Instruction::subfe_d: subfe_d(info); break;
            case Instruction::subfeo: subfeo(info); break;
            case Instruction::subfeo_d: subfeo_d(info); break;
            case Instruction::subfic: subfic(info); break;
            case Instruction::subfme: subfme(info); break;
            case Instruction::subfme_d: subfme_d(info); break;
            case Instruction::subfmeo: subfmeo(info); break;
            case Instruction::subfmeo_d: subfmeo_d(info); break;
            case Instruction::subfze: subfze(info); break;
            case Instruction::subfze_d: subfze_d(info); break;
            case Instruction::subfzeo: subfzeo(info); break;
            case Instruction::subfzeo_d: subfzeo_d(info); break;

            case Instruction::lbz: lbz(info); break;
            case Instruction::lbzu: lbzu(info); break;
            case Instruction::lbzux: lbzux(info); break;
            case Instruction::lbzx: lbzx(info); break;
            case Instruction::lha: lha(info); break;
            case Instruction::lhau: lhau(info); break;
            case Instruction::lhaux: lhaux(info); break;
            case Instruction::lhax: lhax(info); break;
            case Instruction::lhz: lhz(info); break;
            case Instruction::lhzu: lhzu(info); break;
            case Instruction::lhzux: lhzux(info); break;
            case Instruction::lhzx: lhzx(info); break;
            case Instruction::lwz: lwz(info); break;
            case Instruction::lwzu: lwzu(info); break;
            case Instruction::lwzux: lwzux(info); break;
            case Instruction::lwzx: lwzx(info); break;
            case Instruction::stb: stb(info); break;
            case Instruction::stbu: stbu(info); break;
            case Instruction::stbux: stbux(info); break;
            case Instruction::stbx: stbx(info); break;
            case Instruction::sth: sth(info); break;
            case Instruction::sthu: sthu(info); break;
            case Instruction::sthux: sthux(info); break;
            case Instruction::sthx: sthx(info); break;
            case Instruction::stw: stw(info); break;
            case Instruction::stwu: stwu(info); break;
            case Instruction::stwux: stwux(info); break;
            case Instruction::stwx: stwx(info); break;
            case Instruction::lhbrx: lhbrx(info); break;
            case Instruction::lwbrx: lwbrx(info); break;
            case Instruction::sthbrx: sthbrx(info); break;
            case Instruction::stwbrx: stwbrx(info); break;
            case Instruction::lmw: lmw(info); break;
            case Instruction::stmw: stmw(info); break;
            case Instruction::lswi: lswi(info); break;
            case Instruction::lswx: lswx(info); break;
            case Instruction::stswi: stswi(info); break;
            case Instruction::stswx: stswx(info); break;

            case Instruction::_and: _and(info); break;
            case Instruction::and_d: and_d(info); break;
            case Instruction::andc: andc(info); break;
            case Instruction::andc_d: andc_d(info); break;
            case Instruction::andi_d: andi_d(info); break;
            case Instruction::andis_d: andis_d(info); break;
            case Instruction::cntlzw: cntlzw(info); break;
            case Instruction::cntlzw_d: cntlzw_d(info); break;
            case Instruction::eqv: eqv(info); break;
            case Instruction::eqv_d: eqv_d(info); break;
            case Instruction::extsb: extsb(info); break;
            case Instruction::extsb_d: extsb_d(info); break;
            case Instruction::extsh: extsh(info); break;
            case Instruction::extsh_d: extsh_d(info); break;
            case Instruction::nand: nand(info); break;
            case Instruction::nand_d: nand_d(info); break;
            case Instruction::nor: nor(info); break;
            case Instruction::nor_d: nor_d(info); break;
            case Instruction::_or: _or(info); break;
            case Instruction::or_d: or_d(info); break;
            case Instruction::orc: orc(info); break;
            case Instruction::orc_d: orc_d(info); break;
            case Instruction::ori: ori(info); break;
            case Instruction::oris: oris(info); break;
            case Instruction::_xor: _xor(info); break;
            case Instruction::xor_d: xor_d(info); break;
            case Instruction::xori: xori(info); break;
            case Instruction::xoris: xoris(info); break;

            case Instruction::ps_div: ps_div(info); break;
            case Instruction::ps_div_d: ps_div_d(info); break;
            case Instruction::ps_sub: ps_sub(info); break;
            case Instruction::ps_sub_d: ps_sub_d(info); break;
            case Instruction::ps_add: ps_add(info); break;
            case Instruction::ps_add_d: ps_add_d(info); break;
            case Instruction::ps_sel: ps_sel(info); break;
            case Instruction::ps_sel_d: ps_sel_d(info); break;
            case Instruction::ps_res: ps_res(info); break;
            case Instruction::ps_res_d: ps_res_d(info); break;
            case Instruction::ps_mul: ps_mul(info); break;
            case Instruction::ps_mul_d: ps_mul_d(info); break;
            case Instruction::ps_rsqrte: ps_rsqrte(info); break;
            case Instruction::ps_rsqrte_d: ps_rsqrte_d(info); break;
            case Instruction::ps_msub: ps_msub(info); break;
            case Instruction::ps_msub_d: ps_msub_d(info); break;
            case Instruction::ps_madd: ps_madd(info); break;
            case Instruction::ps_madd_d: ps_madd_d(info); break;
            case Instruction::ps_nmsub: ps_nmsub(info); break;
            case Instruction::ps_nmsub_d: ps_nmsub_d(info); break;
            case Instruction::ps_nmadd: ps_nmadd(info); break;
            case Instruction::ps_nmadd_d: ps_nmadd_d(info); break;
            case Instruction::ps_neg: ps_neg(info); break;
            case Instruction::ps_neg_d: ps_neg_d(info); break;
            case Instruction::ps_mr: ps_mr(info); break;
            case Instruction::ps_mr_d: ps_mr_d(info); break;
            case Instruction::ps_nabs: ps_nabs(info); break;
            case Instruction::ps_nabs_d: ps_nabs_d(info); break;
            case Instruction::ps_abs: ps_abs(info); break;
            case Instruction::ps_abs_d: ps_abs_d(info); break;

            case Instruction::ps_sum0: ps_sum0(info); break;
            case Instruction::ps_sum0_d: ps_sum0_d(info); break;
            case Instruction::ps_sum1: ps_sum1(info); break;
            case Instruction::ps_sum1_d: ps_sum1_d(info); break;
            case Instruction::ps_muls0: ps_muls0(info); break;
            case Instruction::ps_muls0_d: ps_muls0_d(info); break;
            case Instruction::ps_muls1: ps_muls1(info); break;
            case Instruction::ps_muls1_d: ps_muls1_d(info); break;
            case Instruction::ps_madds0: ps_madds0(info); break;
            case Instruction::ps_madds0_d: ps_madds0_d(info); break;
            case Instruction::ps_madds1: ps_madds1(info); break;
            case Instruction::ps_madds1_d: ps_madds1_d(info); break;
            case Instruction::ps_cmpu0: ps_cmpu0(info); break;
            case Instruction::ps_cmpo0: ps_cmpo0(info); break;
            case Instruction::ps_cmpu1: ps_cmpu1(info); break;
            case Instruction::ps_cmpo1: ps_cmpo1(info); break;
            case Instruction::ps_merge00: ps_merge00(info); break;
            case Instruction::ps_merge00_d: ps_merge00_d(info); break;
            case Instruction::ps_merge01: ps_merge01(info); break;
            case Instruction::ps_merge01_d: ps_merge01_d(info); break;
            case Instruction::ps_merge10: ps_merge10(info); break;
            case Instruction::ps_merge10_d: ps_merge10_d(info); break;
            case Instruction::ps_merge11: ps_merge11(info); break;
            case Instruction::ps_merge11_d: ps_merge11_d(info); break;

            case Instruction::psq_lx: psq_lx(info); break;
            case Instruction::psq_stx: psq_stx(info); break;
            case Instruction::psq_lux: psq_lux(info); break;
            case Instruction::psq_stux: psq_stux(info); break;
            case Instruction::psq_l: psq_l(info); break;
            case Instruction::psq_lu: psq_lu(info); break;
            case Instruction::psq_st: psq_st(info); break;
            case Instruction::psq_stu: psq_stu(info); break;

            case Instruction::rlwimi: rlwimi(info); break;
            case Instruction::rlwimi_d: rlwimi_d(info); break;
            case Instruction::rlwinm: rlwinm(info); break;
            case Instruction::rlwinm_d: rlwinm_d(info); break;
            case Instruction::rlwnm: rlwnm(info); break;
            case Instruction::rlwnm_d: rlwnm_d(info); break;

            case Instruction::slw: slw(info); break;
            case Instruction::slw_d: slw_d(info); break;
            case Instruction::sraw: sraw(info); break;
            case Instruction::sraw_d: sraw_d(info); break;
            case Instruction::srawi: srawi(info); break;
            case Instruction::srawi_d: srawi_d(info); break;
            case Instruction::srw: srw(info); break;
            case Instruction::srw_d: srw_d(info); break;

            case Instruction::eieio: eieio(info); break;
            case Instruction::isync: isync(info); break;
            case Instruction::lwarx: lwarx(info); break;
            case Instruction::stwcx_d: stwcx_d(info); break;
            case Instruction::sync: sync(info); break;
            case Instruction::rfi: rfi(info); break;
            case Instruction::sc: sc(info); break;
            case Instruction::tw: tw(info); break;
            case Instruction::twi: twi(info); break;
            case Instruction::mcrxr: mcrxr(info); break;
            case Instruction::mfcr: mfcr(info); break;
            case Instruction::mfmsr: mfmsr(info); break;
            case Instruction::mfspr: mfspr(info); break;
            case Instruction::mftb: mftb(info); break;
            case Instruction::mtcrf: mtcrf(info); break;
            case Instruction::mtmsr: mtmsr(info); break;
            case Instruction::mtspr: mtspr(info); break;
            case Instruction::dcbf: dcbf(info); break;
            case Instruction::dcbi: dcbi(info); break;
            case Instruction::dcbst: dcbst(info); break;
            case Instruction::dcbt: dcbt(info); break;
            case Instruction::dcbtst: dcbtst(info); break;
            case Instruction::dcbz: dcbz(info); break;
            case Instruction::dcbz_l: dcbz_l(info); break;
            case Instruction::icbi: icbi(info); break;
            case Instruction::mfsr: mfsr(info); break;
            case Instruction::mfsrin: mfsrin(info); break;
            case Instruction::mtsr: mtsr(info); break;
            case Instruction::mtsrin: mtsrin(info); break;
            case Instruction::tlbie: tlbie(info); break;
            case Instruction::tlbsync: tlbsync(info); break;
            case Instruction::eciwx: eciwx(info); break;
            case Instruction::ecowx: ecowx(info); break;

            // TODO: CallVM opcode.

            default:
                Debug::Halt("** CPU ERROR **\n"
                    "unimplemented opcode : %08X\n", core->regs.pc);

                Gekko->PrCause = PrivilegedCause::IllegalInstruction;
                Gekko->Exception(Exception::PROGRAM);
                return;
        }

        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)info.instr]++;
        }
    }

    uint32_t Interpreter::GetRotMask(int mb, int me)
    {
        return rotmask[mb][me];
    }

    // high level call
    void Interpreter::callvm(AnalyzeInfo& info)
    {
        // Dolwin module base should be specified as 0x400000 in project properties
        //void (*pcall)() = (void (*)())((void*)(uint64_t)op);

        //if (op == 0)
        //{
        //	Halt(
        //		"Something goes wrong in interpreter, \n"
        //		"program is trying to execute NULL opcode.\n\n"
        //		"pc:%08X", Gekko->regs.pc);
        //	return;
        //}

        //pcall();

        Debug::Halt("callvm: Temporary not implemented!\n");
    }

}
