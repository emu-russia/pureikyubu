// Standalone Gekko interpreter benchmark.
//
// Loads a DOL into a flat 24 MB main memory, sets up the state the IPL leaves
// behind, and runs GekkoCore::Step() in a tight loop (the same way the Windows
// build's RingleaderThreadProc does).

#include "pch.h"
#include <chrono>
#include <fstream>
#include <algorithm>
#include <set>
#include <csetjmp>

void ProfStart();
void ProfStop(const char* path);

Gekko::GekkoCore* Core = nullptr;

// ---------------------------------------------------------------------------
// DOL format

namespace DOL
{
	struct Header
	{
		uint32_t textOffset[7];
		uint32_t dataOffset[11];
		uint32_t textAddress[7];
		uint32_t dataAddress[11];
		uint32_t textSize[7];
		uint32_t dataSize[11];
		uint32_t bssAddress;
		uint32_t bssSize;
		uint32_t entryPoint;
		uint32_t padd[7];
	};

	static inline uint32_t Swap32(uint32_t v)
	{
		return _BYTESWAP_UINT32(v);
	}

	static bool Load(const char* path, uint8_t* ram, uint32_t ramSize, uint32_t& entry)
	{
		std::ifstream f(path, std::ifstream::binary);
		if (!f.is_open()) return false;

		Header h{};
		f.read((char*)&h, sizeof(h));
		if (!f) return false;

		for (int i = 0; i < 7; i++) { h.textOffset[i] = Swap32(h.textOffset[i]); h.textAddress[i] = Swap32(h.textAddress[i]); h.textSize[i] = Swap32(h.textSize[i]); }
		for (int i = 0; i < 11; i++) { h.dataOffset[i] = Swap32(h.dataOffset[i]); h.dataAddress[i] = Swap32(h.dataAddress[i]); h.dataSize[i] = Swap32(h.dataSize[i]); }
		h.bssAddress = Swap32(h.bssAddress);
		h.bssSize = Swap32(h.bssSize);
		h.entryPoint = Swap32(h.entryPoint);

		for (int i = 0; i < 7; i++)
		{
			if (!h.textOffset[i]) continue;
			uint32_t pa = h.textAddress[i] & 0x03ff'ffff;
			if (pa + h.textSize[i] > ramSize) { printf("text section out of RAM\n"); return false; }
			f.seekg(h.textOffset[i]);
			f.read((char*)&ram[pa], h.textSize[i]);
		}
		for (int i = 0; i < 11; i++)
		{
			if (!h.dataOffset[i]) continue;
			uint32_t pa = h.dataAddress[i] & 0x03ff'ffff;
			if (pa + h.dataSize[i] > ramSize) { printf("data section out of RAM\n"); return false; }
			f.seekg(h.dataOffset[i]);
			f.read((char*)&ram[pa], h.dataSize[i]);
		}

		entry = h.entryPoint;
		return true;
	}
}

// ---------------------------------------------------------------------------
// State fingerprint (to prove that an optimisation does not change behaviour)

static uint64_t HashState(Gekko::GekkoCore* core, uint8_t* ram, uint32_t ramSize)
{
	uint64_t h = 1469598103934665603ull;
	auto mix = [&h](uint64_t v) { h ^= v; h *= 1099511628211ull; };

	for (int i = 0; i < 32; i++) mix(core->regs.gpr[i]);
	for (int i = 0; i < 1024; i++) mix(core->regs.spr[i]);
	for (int i = 0; i < 32; i++) mix(core->regs.fpr[i].uval);
	for (int i = 0; i < 32; i++) mix(core->regs.ps1[i].uval);
	mix(core->regs.cr);
	mix(core->regs.msr);
	mix(core->regs.fpscr);
	mix(core->regs.pc);
	mix(core->regs.tb.uval);
	mix(core->regs.sr[0]);

	// A cheap RAM fingerprint (word-wise, so it is endianness-independent enough)
	for (uint32_t a = 0; a < ramSize; a += 4096)
	{
		mix(*(uint32_t*)&ram[a]);
	}
	return h;
}


// ---------------------------------------------------------------------------
// Random instruction fuzzer: builds a linear program from random instruction
// words and checks that the recompiler produces exactly the same state as the
// interpreter.

static uint64_t FuzzNext(uint64_t& s)
{
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return s;
}

// Set by BENCH_FUZZ_PS: include the Paired-Single arithmetic and seed the
// floating point registers (see FuzzUnsafe and RunFuzzer::init).
static bool g_fuzzPs = false;

// The recompiler is selected by BENCH_JIT. An empty value counts as "not
// selected", so that "BENCH_JIT= <bin>" cannot silently measure the recompiler
// twice - which is exactly the sort of thing a numbers-only comparison hides.
static bool JitRequested()
{
    const char* v = getenv("BENCH_JIT");
    return v != nullptr && v[0] != '\0';
}

static bool FuzzUnsafe(Gekko::Instruction i)
{
    using Gekko::Instruction;

    // BENCH_FUZZ_PS lets the Paired-Single arithmetic through: the point of that
    // mode is to compare the SSE translations in gekkojit_ps.cpp against the
    // interpreter, and the FPRs are seeded for it (see init()). The quantised
    // loads and stores and the comparison forms stay out - they are not translated
    // yet, so they would only exercise the fallback.
    if (g_fuzzPs)
    {
        switch (i)
        {
        case Instruction::ps_add: case Instruction::ps_add_d: case Instruction::ps_sub: case Instruction::ps_sub_d:
        case Instruction::ps_mul: case Instruction::ps_mul_d: case Instruction::ps_div: case Instruction::ps_div_d:
        case Instruction::ps_res: case Instruction::ps_res_d: case Instruction::ps_rsqrte: case Instruction::ps_rsqrte_d:
        case Instruction::ps_sel: case Instruction::ps_sel_d: case Instruction::ps_muls0: case Instruction::ps_muls0_d:
        case Instruction::ps_muls1: case Instruction::ps_muls1_d: case Instruction::ps_sum0: case Instruction::ps_sum0_d:
        case Instruction::ps_sum1: case Instruction::ps_sum1_d: case Instruction::ps_madd: case Instruction::ps_madd_d:
        case Instruction::ps_msub: case Instruction::ps_msub_d: case Instruction::ps_nmadd: case Instruction::ps_nmadd_d:
        case Instruction::ps_nmsub: case Instruction::ps_nmsub_d: case Instruction::ps_madds0: case Instruction::ps_madds0_d:
        case Instruction::ps_madds1: case Instruction::ps_madds1_d:
        case Instruction::ps_mr: case Instruction::ps_mr_d: case Instruction::ps_neg: case Instruction::ps_neg_d:
        case Instruction::ps_abs: case Instruction::ps_abs_d: case Instruction::ps_nabs: case Instruction::ps_nabs_d:
        case Instruction::ps_merge00: case Instruction::ps_merge00_d: case Instruction::ps_merge01: case Instruction::ps_merge01_d:
        case Instruction::ps_merge10: case Instruction::ps_merge10_d: case Instruction::ps_merge11: case Instruction::ps_merge11_d:
            return false;
        default:
            break;
        }
    }

    switch (i)
    {
    case Instruction::rfi: case Instruction::sc: case Instruction::tw: case Instruction::twi:
    case Instruction::mtspr: case Instruction::mtmsr: case Instruction::mfspr: case Instruction::mftb:
    case Instruction::eciwx: case Instruction::ecowx: case Instruction::tlbie: case Instruction::tlbsync:
    case Instruction::dcbi: case Instruction::dcbf: case Instruction::icbi: case Instruction::dcbz_l:
    // TEMP: floating point excluded for bisection
    case Instruction::fadd: case Instruction::fadd_d: case Instruction::fadds: case Instruction::fadds_d:
    case Instruction::fsub: case Instruction::fsub_d: case Instruction::fsubs: case Instruction::fsubs_d:
    case Instruction::fmul: case Instruction::fmul_d: case Instruction::fmuls: case Instruction::fmuls_d:
    case Instruction::fdiv: case Instruction::fdiv_d: case Instruction::fdivs: case Instruction::fdivs_d:
    case Instruction::fres: case Instruction::fres_d: case Instruction::frsqrte: case Instruction::frsqrte_d:
    case Instruction::fsel: case Instruction::fsel_d:
    case Instruction::fmadd: case Instruction::fmadd_d: case Instruction::fmadds: case Instruction::fmadds_d:
    case Instruction::fmsub: case Instruction::fmsub_d: case Instruction::fmsubs: case Instruction::fmsubs_d:
    case Instruction::fnmadd: case Instruction::fnmadd_d: case Instruction::fnmadds: case Instruction::fnmadds_d:
    case Instruction::fnmsub: case Instruction::fnmsub_d: case Instruction::fnmsubs: case Instruction::fnmsubs_d:
    case Instruction::frsp: case Instruction::frsp_d: case Instruction::fctiw: case Instruction::fctiw_d:
    case Instruction::fctiwz: case Instruction::fctiwz_d: case Instruction::fcmpu: case Instruction::fcmpo:
    case Instruction::mffs: case Instruction::mffs_d: case Instruction::mcrfs:
    case Instruction::mtfsfi: case Instruction::mtfsfi_d: case Instruction::mtfsf: case Instruction::mtfsf_d:
    case Instruction::mtfsb0: case Instruction::mtfsb0_d: case Instruction::mtfsb1: case Instruction::mtfsb1_d:
    case Instruction::fmr: case Instruction::fmr_d: case Instruction::fneg: case Instruction::fneg_d:
    case Instruction::fabs: case Instruction::fabs_d: case Instruction::fnabs: case Instruction::fnabs_d:
    case Instruction::lfs: case Instruction::lfsx: case Instruction::lfsu: case Instruction::lfsux:
    case Instruction::lfd: case Instruction::lfdx: case Instruction::lfdu: case Instruction::lfdux:
    case Instruction::stfs: case Instruction::stfsx: case Instruction::stfsu: case Instruction::stfsux:
    case Instruction::stfd: case Instruction::stfdx: case Instruction::stfdu: case Instruction::stfdux:
    case Instruction::stfiwx:
    case Instruction::ps_add: case Instruction::ps_add_d: case Instruction::ps_sub: case Instruction::ps_sub_d:
    case Instruction::ps_mul: case Instruction::ps_mul_d: case Instruction::ps_div: case Instruction::ps_div_d:
    case Instruction::ps_res: case Instruction::ps_res_d: case Instruction::ps_rsqrte: case Instruction::ps_rsqrte_d:
    case Instruction::ps_sel: case Instruction::ps_sel_d: case Instruction::ps_muls0: case Instruction::ps_muls0_d:
    case Instruction::ps_muls1: case Instruction::ps_muls1_d: case Instruction::ps_sum0: case Instruction::ps_sum0_d:
    case Instruction::ps_sum1: case Instruction::ps_sum1_d: case Instruction::ps_madd: case Instruction::ps_madd_d:
    case Instruction::ps_msub: case Instruction::ps_msub_d: case Instruction::ps_nmadd: case Instruction::ps_nmadd_d:
    case Instruction::ps_nmsub: case Instruction::ps_nmsub_d: case Instruction::ps_madds0: case Instruction::ps_madds0_d:
    case Instruction::ps_madds1: case Instruction::ps_madds1_d:
    case Instruction::ps_cmpu0: case Instruction::ps_cmpu1: case Instruction::ps_cmpo0: case Instruction::ps_cmpo1:
    case Instruction::ps_mr: case Instruction::ps_mr_d: case Instruction::ps_neg: case Instruction::ps_neg_d:
    case Instruction::ps_abs: case Instruction::ps_abs_d: case Instruction::ps_nabs: case Instruction::ps_nabs_d:
    case Instruction::ps_merge00: case Instruction::ps_merge00_d: case Instruction::ps_merge01: case Instruction::ps_merge01_d:
    case Instruction::ps_merge10: case Instruction::ps_merge10_d: case Instruction::ps_merge11: case Instruction::ps_merge11_d:
    case Instruction::psq_l: case Instruction::psq_lx: case Instruction::psq_lu: case Instruction::psq_lux:
    case Instruction::psq_st: case Instruction::psq_stx: case Instruction::psq_stu: case Instruction::psq_stux:
    // Branches are exercised: gen() redirects relative displacements into the
    // program and seeds LR/CTR to pc+4. Only the absolute conditional forms are
    // excluded, because their 16 bit target cannot reach the program.
    case Instruction::bca: case Instruction::bcla:
        return true;
    default:
        return false;
    }
}

static uint64_t HashRegs(Gekko::GekkoCore* core)
{
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint64_t v) { h ^= v; h *= 1099511628211ull; };
    for (int i = 0; i < 32; i++) mix(core->regs.gpr[i]);
    for (int i = 0; i < 32; i++) mix(core->regs.fpr[i].uval);
    for (int i = 0; i < 32; i++) mix(core->regs.ps1[i].uval);
    for (int i = 0; i < 64; i++) mix(core->regs.spr[i]);
    mix(core->regs.cr); mix(core->regs.msr); mix(core->regs.fpscr); mix(core->regs.pc);
    mix(core->regs.tb.uval);
    return h;
}

extern jmp_buf g_fuzzJmp;
extern bool g_fuzzActive;

static int RunFuzzer(uint8_t* ram, int iterations, int perProgram)
{
    const uint32_t dataBase = 0x80080000;
    const uint32_t codeBase = 0x80200000;
    int failures = 0;
    int skipped = 0;
    std::set<int> covered;
    // Fill the whole code area with nops. A branch that goes somewhere unintended
    // then keeps executing instead of running off into zeros (which would just stop
    // the run and hide the divergence), so the two engines can be compared.
    for (uint32_t pa = 0x200000; pa < 0x800000; pa += 4)
        *(uint32_t*)&ram[pa] = _BYTESWAP_UINT32(0x60000000);

    int from = getenv("BENCH_FUZZ_FROM") ? atoi(getenv("BENCH_FUZZ_FROM")) : 0;
    int until = getenv("BENCH_FUZZ_UNTIL") ? atoi(getenv("BENCH_FUZZ_UNTIL")) : iterations;
    g_fuzzPs = getenv("BENCH_FUZZ_PS") != nullptr;

    for (int it = from; it < until; it++)
    {
        uint64_t seed = 0x0F1E2D3C4B5A6978ull + (uint64_t)it * 2654435761u;

        std::vector<uint32_t> prog;
        while ((int)prog.size() < perProgram)
        {
            uint32_t w = (uint32_t)FuzzNext(seed);
            Gekko::DecoderInfo di;
            Gekko::Decoder::Decode(0, w, &di);
            if (di.instr == Gekko::Instruction::Unknown) continue;
            if (FuzzUnsafe(di.instr)) continue;
            // Keep relative branches inside the program: their displacement is
            // redirected to pc+8, and LR/CTR are seeded to pc+4 by init().
            switch (di.instr)
            {
            case Gekko::Instruction::b:
            case Gekko::Instruction::bl:
                w = (w & 0xFC000003u) | 8u;
                break;
            case Gekko::Instruction::bc:
            case Gekko::Instruction::bcl:
                w = (w & 0xFFFF0003u) | 8u;
                break;
            default:
                break;
            }

            prog.push_back(w);
            covered.insert((int)di.instr);
        }

        uint32_t codePa = (codeBase + (uint32_t)it * 0x1000) & 0x03ff'ffff;
        uint64_t initSeed = seed;			// both engines must start from the same state

        // Exception vectors: skip the faulting instruction and return, so that a
        // random program that addresses unmapped memory is still executed by both
        // engines instead of stopping the run.
        static const uint32_t excHandler[4] = { 0x7D6B02A6, 0x396B0004, 0x7D6B03A6, 0x4C000064 };	// mfspr r11,SRR0; addi r11,r11,4; mtspr SRR0,r11; rfi

        auto init = [&](bool jit)
        {
            // Cover every address the random registers can reach, not just the base:
            // the two engines must start from byte-identical memory.
            memset(ram + 0x70000, 0, 0x130000 - 0x70000);
            for (int v = 0; v < 16; v++)
            {
                uint32_t vec = (uint32_t)v * 0x100;
                for (int k = 0; k < 4; k++)
                    *(uint32_t*)&ram[vec + k * 4] = _BYTESWAP_UINT32(excHandler[k]);
            }
            for (int i = 0; i < (int)prog.size(); i++)
                *(uint32_t*)&ram[codePa + i * 4] = _BYTESWAP_UINT32(prog[i]);

            Core->Reset();						// clears jit blocks and the caches
            Core->cache->Enable(true);
            Core->icache->Enable(true);

            for (int i = 0; i < 16; i++) Core->regs.sr[i] = 0x80000000;
            Core->regs.spr[(int)Gekko::SPR::DBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::DBAT0L] = 0x00000002;
            Core->regs.spr[(int)Gekko::SPR::IBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::IBAT0L] = 0x00000002;
            Core->regs.msr = MSR_IR | MSR_DR | MSR_FP;

            uint64_t s2 = initSeed;
            for (int i = 0; i < 32; i++)
                Core->regs.gpr[i] = dataBase + (uint32_t)(FuzzNext(s2) & 0xFFFF);
            Core->regs.cr = (uint32_t)FuzzNext(s2);
            Core->regs.spr[(int)Gekko::SPR::XER] = (uint32_t)FuzzNext(s2);
            if (g_fuzzPs)
            {
                // Raw bit patterns, so that NaN, infinity, denormals and the sign
                // of zero are all compared between the two engines as well.
                for (int i = 0; i < 32; i++)
                {
                    Core->regs.fpr[i].uval = FuzzNext(s2);
                    Core->regs.ps1[i].uval = FuzzNext(s2);
                }
                Core->regs.fpscr = (uint32_t)FuzzNext(s2);
            }
            Core->regs.pc = codeBase + (uint32_t)it * 0x1000;
            // Branches through LR/CTR must land inside the program.
            Core->regs.spr[(int)Gekko::SPR::LR] = Core->regs.pc + 4;
            Core->regs.spr[(int)Gekko::SPR::CTR] = Core->regs.pc + 4;
            Core->regs.tb.uval = 0;
        };

        if (setjmp(g_fuzzJmp))
        {
            g_fuzzActive = false;
            skipped++;
            continue;
        }
        g_fuzzActive = true;

        // The recompiler retires whole blocks, so it may overshoot the requested
        // instruction count. Run it first, then run the interpreter for exactly the
        // same number of instructions.
        int traceIter = getenv("BENCH_FUZZ_TRACE") ? atoi(getenv("BENCH_FUZZ_TRACE")) : -1;

        bool noJit = getenv("BENCH_FUZZ_NOJIT") != nullptr;
        init(true);
        if (it == traceIter) printf("  AFTER INIT f13=%016llX f12=%016llX\n", (unsigned long long)Core->regs.fpr[13].uval, (unsigned long long)Core->regs.fpr[12].uval);
        int guard = 0;
        while ((int)Core->GetInstructionCounter() < perProgram && guard++ < 1000)
        {
#if defined(BENCH_WITH_JIT)
            if (noJit) Core->Step(); else Core->jit->Run();
#else
            (void)noJit; Core->Step();
#endif
            if (it == traceIter) {
                uint64_t fh = 1469598103934665603ull;
                for (int g = 0; g < 32; g++) { fh ^= Core->regs.fpr[g].uval; fh *= 1099511628211ull; }
                printf("  JIT  pc=%08X cr=%08X sr0=%08X ops=%lld fh=%016llX f13=%016llX\n", Core->regs.pc,
                Core->regs.cr, Core->regs.spr[(int)Gekko::SPR::SRR0], (long long)Core->GetInstructionCounter(), (unsigned long long)fh,
                (unsigned long long)Core->regs.fpr[13].uval);
                if (getenv("BENCH_FUZZ_DUMP") && Core->GetInstructionCounter() == atoi(getenv("BENCH_FUZZ_DUMP")))
                    for (int g = 0; g < 32; g++) printf("    J f%d=%016llX\n", g, (unsigned long long)Core->regs.fpr[g].uval);
            }
        }
        uint64_t hj = HashRegs(Core);

        uint32_t jGpr[32]; uint32_t jSpr[128]; uint32_t jCr, jMsr, jPc; uint64_t jTb;
        uint64_t jFpr[32], jPs1[32]; uint32_t jFpscr;
        memcpy(jGpr, Core->regs.gpr, sizeof(jGpr));
        memcpy(jSpr, Core->regs.spr, sizeof(jSpr));
        for (int i = 0; i < 32; i++) { jFpr[i] = Core->regs.fpr[i].uval; jPs1[i] = Core->regs.ps1[i].uval; }
        jFpscr = Core->regs.fpscr;
        jCr = Core->regs.cr; jMsr = Core->regs.msr; jPc = Core->regs.pc; jTb = Core->regs.tb.uval;
        uint64_t jn = Core->GetInstructionCounter();

        init(false);
        guard = 0;
        while ((int)Core->GetInstructionCounter() < (int)jn && guard++ < 10000)
        {
            Core->Step();
            if (it == traceIter) {
                uint64_t fh = 1469598103934665603ull;
                for (int g = 0; g < 32; g++) { fh ^= Core->regs.fpr[g].uval; fh *= 1099511628211ull; }
                printf("  INT  pc=%08X cr=%08X sr0=%08X ops=%lld fh=%016llX f13=%016llX\n", Core->regs.pc,
                Core->regs.cr, Core->regs.spr[(int)Gekko::SPR::SRR0], (long long)Core->GetInstructionCounter(), (unsigned long long)fh,
                (unsigned long long)Core->regs.fpr[13].uval);
                if (getenv("BENCH_FUZZ_DUMP") && Core->GetInstructionCounter() == atoi(getenv("BENCH_FUZZ_DUMP")))
                    for (int g = 0; g < 32; g++) printf("    I f%d=%016llX\n", g, (unsigned long long)Core->regs.fpr[g].uval);
            }
        }
        uint64_t hi = HashRegs(Core);

        if (hi != hj && failures < 3)
        {
            printf("   jn=%llu\n", (unsigned long long)jn);
            for (int g = 0; g < 32; g++)
                if (jGpr[g] != Core->regs.gpr[g]) printf("   gpr[%d] interp=%08X jit=%08X\n", g, Core->regs.gpr[g], jGpr[g]);
            for (int g = 0; g < 128; g++)
                if (jSpr[g] != Core->regs.spr[g]) printf("   spr[%d] interp=%08X jit=%08X\n", g, Core->regs.spr[g], jSpr[g]);
            if (jCr != Core->regs.cr) printf("   cr interp=%08X jit=%08X\n", Core->regs.cr, jCr);
            if (jMsr != Core->regs.msr) printf("   msr interp=%08X jit=%08X\n", Core->regs.msr, jMsr);
            if (jPc != Core->regs.pc) printf("   pc interp=%08X jit=%08X\n", Core->regs.pc, jPc);
            if (jTb != Core->regs.tb.uval) printf("   tb interp=%llu jit=%llu\n", (unsigned long long)Core->regs.tb.uval, (unsigned long long)jTb);
            for (int g = 0; g < 32; g++)
            {
                if (jFpr[g] != Core->regs.fpr[g].uval) printf("   fpr[%d] interp=%016llX jit=%016llX\n", g, (unsigned long long)Core->regs.fpr[g].uval, (unsigned long long)jFpr[g]);
                if (jPs1[g] != Core->regs.ps1[g].uval) printf("   ps1[%d] interp=%016llX jit=%016llX\n", g, (unsigned long long)Core->regs.ps1[g].uval, (unsigned long long)jPs1[g]);
            }
            if (jFpscr != Core->regs.fpscr) printf("   fpscr interp=%08X jit=%08X\n", Core->regs.fpscr, jFpscr);
        }

        if (hi != hj)
        {
            if (failures < 5)
            {
                printf("FUZZ FAIL iter %d: interp=%016llX jit=%016llX pc=%08X\n",
                    it, (unsigned long long)hi, (unsigned long long)hj, Core->regs.pc);
                for (size_t k = 0; k < prog.size(); k++)
                {
                    Gekko::DecoderInfo d2;
                    Gekko::Decoder::Decode(0, prog[k], &d2);
                    printf("  %08X  %s\n", prog[k], Gekko::GekkoDisasm::InstrToString(&d2).c_str());
                }
            }
            failures++;
        }
    }

    printf("fuzzer: %d programs (%d skipped by a guest exception), %zu distinct instruction types, %d failures\n",
        iterations, skipped, covered.size(), failures);
    return failures;
}


// ---------------------------------------------------------------------------
// Boot path test: runs the synthetic IPL (boot ROM at 0xFFF00100 with translation
// off) which sets up the BATs, enables the caches and MSR[IR]/[DR], flushes an
// instruction range line by line and enters the game at 0x80001000.

static bool LoadBlob(const char* path, uint8_t* dst, size_t maxSize, size_t& size)
{
    std::ifstream f(path, std::ifstream::binary);
    if (!f.is_open()) return false;
    f.read((char*)dst, maxSize);
    size = (size_t)f.gcount();
    return size > 0;
}

static int RunIpl(const char* prefix, uint64_t steps, bool jit)
{
    static uint8_t boot[256 * 1024];
    static uint8_t game[256 * 1024];
    size_t bootSize = 0, gameSize = 0;

    std::string bp = std::string(prefix) + "_bootrom.bin";
    std::string gp = std::string(prefix) + "_game.bin";
    if (!LoadBlob(bp.c_str(), boot, sizeof(boot), bootSize) ||
        !LoadBlob(gp.c_str(), game, sizeof(game), gameSize))
    {
        printf("cannot load %s / %s\n", bp.c_str(), gp.c_str());
        return 1;
    }

    // The real image covers 0xFFF00000..0xFFFFFFFF and the IPL entry is at
    // 0xFFF00100, so the code goes at offset 0x100 of the image.
    memmove(boot + 0x100, boot, bootSize);
    memset(boot, 0, 0x100);
    Flipper::ProcessorInterface::bootrom = boot;
    Flipper::ProcessorInterface::bootromSize = (uint32_t)bootSize + 0x100;

    uint8_t* ram = Flipper::ProcessorInterface::ram;
    memcpy(&ram[0x1000], game, gameSize);

    // Exception vectors: skip the faulting instruction and return (rfi).
    static const uint32_t excHandler[4] = { 0x7D6B02A6, 0x396B0004, 0x7D6B03A6, 0x4C000064 };

    Core->Reset();
    // Power-on state: translation off, caches off (BootROM() does the rest).
    for (int i = 0; i < 16; i++) Core->regs.sr[i] = 0x80000000;
    for (int v = 0; v < 16; v++)
        for (int k = 0; k < 4; k++)
            *(uint32_t*)&ram[v * 0x100 + k * 4] = _BYTESWAP_UINT32(excHandler[k]);
    Core->regs.msr = MSR_FP;
    Core->regs.gpr[1] = 0x816ffffc;
    Core->regs.pc = 0xFFF00100;

    bool trace = getenv("BENCH_IPL_TRACE") != nullptr;
    uint64_t t0 = (uint64_t)Core->GetInstructionCounter();
    while ((uint64_t)Core->GetInstructionCounter() - t0 < steps)
    {
        if (trace && ((uint64_t)Core->GetInstructionCounter() - t0) < 400)
            printf("  %llu pc=%08X msr=%08X\n", (unsigned long long)((uint64_t)Core->GetInstructionCounter() - t0),
                Core->regs.pc, Core->regs.msr);
        if (Core->regs.pc >= 0x80000000 && Core->regs.pc < 0x8C000000)
            break;						// reached the game and finished the IPL
#if defined(BENCH_WITH_JIT)
        if (jit) Core->jit->Run(); else Core->Step();
#else
        (void)jit; Core->Step();
#endif
    }

    printf("%s: ipl done at pc=%08X after %llu instructions, msr=%08X cr=%08X tb=%llu\n",
        jit ? "JIT" : "INTERP", Core->regs.pc,
        (unsigned long long)((uint64_t)Core->GetInstructionCounter() - t0),
        Core->regs.msr, Core->regs.cr, (unsigned long long)Core->regs.tb.uval);

    // Now run the game itself.
    uint64_t g0 = (uint64_t)Core->GetInstructionCounter();
    uint64_t gameSteps = getenv("BENCH_GAME_STEPS") ? strtoull(getenv("BENCH_GAME_STEPS"), nullptr, 0) : steps;
    while ((uint64_t)Core->GetInstructionCounter() - g0 < gameSteps)
    {
#if defined(BENCH_WITH_JIT)
        if (jit) Core->jit->Run(); else Core->Step();
#else
        (void)jit; Core->Step();
#endif
    }

    printf("%s: game pc=%08X cr=%08X tb=%llu hash=%016llX\n",
        jit ? "JIT" : "INTERP", Core->regs.pc, Core->regs.cr,
        (unsigned long long)Core->regs.tb.uval, (unsigned long long)HashState(Core, ram, 24 * 1024 * 1024));
    return 0;
}


// ---------------------------------------------------------------------------
// Real IPL: loads a real boot ROM image (the repo's Data/gc-ntsc-11.bin), runs the
// descrambler from bootrtc.cpp and starts at 0xFFF00100 from the power-on state.

static void IplDescramble(uint8_t* data, size_t size)
{
    uint8_t acc = 0, nacc = 0;
    uint16_t t = 0x2953, u = 0xd9c2, v = 0x3ff1;
    uint8_t x = 1;

    for (size_t it = 0; it < size;)
    {
        int t0 = t & 1, t1 = (t >> 1) & 1;
        int u0 = u & 1, u1 = (u >> 1) & 1;
        int v0 = v & 1;

        x ^= t1 ^ v0;
        x ^= (u0 | u1);
        x ^= (t0 ^ u1 ^ v0) & (t0 ^ u0);

        if (t0 == u0) { v >>= 1; if (v0) v ^= 0xb3d0; }
        if (t0 == 0) { u >>= 1; if (u0) u ^= 0xfb10; }
        t >>= 1;
        if (t0) t ^= 0xa740;

        nacc++;
        acc = 2 * acc + x;
        if (nacc == 8) { data[it++] ^= acc; nacc = 0; }
    }
}

static int RunRealIpl(const char* imagePath, uint64_t steps, bool jit)
{
    const size_t bootromSize = 2 * 1024 * 1024;
    static uint8_t image[bootromSize];

    std::ifstream f(imagePath, std::ifstream::binary);
    if (!f.is_open()) { printf("cannot open %s\n", imagePath); return 1; }
    f.read((char*)image, bootromSize);
    if ((size_t)f.gcount() != bootromSize) { printf("%s is not a 2 MB boot ROM\n", imagePath); return 1; }

    // Find the end of the scrambled region, then descramble it.
    size_t beginOffset = 0x100, endOffset = bootromSize - 0x20, offset = beginOffset;
    uint8_t zero[0x20] = { 0 };
    while (offset < endOffset) { if (memcmp(&image[offset], zero, 0x20) == 0) break; offset += 0x20; }
    if (offset == endOffset) { printf("no empty cache line found\n"); return 1; }
    IplDescramble(&image[beginOffset], offset - beginOffset);

    Flipper::ProcessorInterface::bootrom = image;
    Flipper::ProcessorInterface::bootromSize = (uint32_t)bootromSize;

    uint8_t* ram = Flipper::ProcessorInterface::ram;

    extern jmp_buf g_fuzzJmp;
    extern bool g_fuzzActive;

    if (setjmp(g_fuzzJmp))
    {
        g_fuzzActive = false;
        printf("%s: HALT after %llu instructions, pc=%08X lr=%08X msr=%08X\n",
            jit ? "JIT" : "INTERP", (unsigned long long)Core->GetInstructionCounter(),
            Core->regs.pc, Core->regs.spr[(int)Gekko::SPR::LR], Core->regs.msr);
        return 2;
    }
    g_fuzzActive = true;

    Core->Reset();
    Flipper::ProcessorInterface::ram = ram;
    Core->regs.pc = 0xFFF00100;			// power-on: translation off, caches off

    uint64_t iplSteps = getenv("BENCH_IPL_STEPS") ? strtoull(getenv("BENCH_IPL_STEPS"), nullptr, 0) : steps;

    uint64_t last = 0;
    while ((uint64_t)Core->GetInstructionCounter() < iplSteps)
    {
#if defined(BENCH_WITH_JIT)
        if (jit) Core->jit->Run(); else Core->Step();
#else
        (void)jit; Core->Step();
#endif
        uint64_t n = (uint64_t)Core->GetInstructionCounter();
        if (getenv("BENCH_IPL_TRACE") && n / 100000 != last / 100000)
            printf("  %8llu pc=%08X msr=%08X lr=%08X\n", (unsigned long long)n,
                Core->regs.pc, Core->regs.msr, Core->regs.spr[(int)Gekko::SPR::LR]);
        last = n;
    }

    // Simulate what the IPL does next: EXI copies IPL2 straight into main memory
    // (behind the CPU's back, so the data cache keeps whatever was there before) and
    // the core jumps to it at its cached effective address.
    if (const char* ipl2 = getenv("BENCH_IPL2_BIN"))
    {
        static uint8_t blob[64 * 1024];
        size_t size = 0;
        if (!LoadBlob(ipl2, blob, sizeof(blob), size)) { printf("cannot open %s\n", ipl2); return 1; }

        // Dirty the data cache for that region first, the way the memory size probe
        // does, so that a stale line really is there.
        for (uint32_t pa = 0x1300000; pa < 0x1300000 + 0x4000; pa += 4)
            *(uint32_t*)&ram[pa] = _BYTESWAP_UINT32(0x55555555);
        for (uint32_t pa = 0x1300000; pa < 0x1300000 + 0x4000; pa += 4)
            Core->WriteWord(0x81300000 + (pa - 0x1300000), 0x55555555);

        memcpy(&ram[0x1300000], blob, size);		// EXI DMA: bypasses the caches
        Core->regs.pc = 0x81300000;

        uint64_t ipl2Steps = getenv("BENCH_IPL2_STEPS") ? strtoull(getenv("BENCH_IPL2_STEPS"), nullptr, 0) : 200000;
        uint64_t base = (uint64_t)Core->GetInstructionCounter();
        while ((uint64_t)Core->GetInstructionCounter() - base < ipl2Steps)
        {
#if defined(BENCH_WITH_JIT)
            if (jit) Core->jit->Run(); else Core->Step();
#else
            (void)jit; Core->Step();
#endif
        }
    }

    g_fuzzActive = false;
    printf("%s: %llu instructions, pc=%08X msr=%08X cr=%08X tb=%llu hash=%016llX\n",
        jit ? "JIT" : "INTERP", (unsigned long long)Core->GetInstructionCounter(),
        Core->regs.pc, Core->regs.msr, Core->regs.cr, (unsigned long long)Core->regs.tb.uval,
        (unsigned long long)HashState(Core, ram, 24 * 1024 * 1024));
    return 0;
}


// ---------------------------------------------------------------------------
// Exhaustive branch test: every branch form, all interesting BO/BI combinations
// and several LR/CTR values, one instruction per case, interpreter vs recompiler.

static int BranchTest()
{
    const uint32_t codePa = 0x200000, effPc = 0x80200000;
    uint8_t* ram = Flipper::ProcessorInterface::ram;

    // 8 nops after the branch so that either outcome lands on real code.
    for (int i = 1; i < 12; i++)
        *(uint32_t*)&ram[codePa + i * 4] = _BYTESWAP_UINT32(0x60000000);

    struct Case { const char* name; uint32_t word; };
    std::vector<Case> cases;

    auto b   = [](uint32_t off) { return (18u << 26) | (off & 0x03FFFFFCu); };
    auto bc  = [](uint32_t bo, uint32_t bi, uint32_t bd) { return (16u << 26) | (bo << 21) | (bi << 16) | (bd & 0xFFFCu); };
    auto bbx = [](uint32_t bo, uint32_t bi, uint32_t xo) { return (19u << 26) | (bo << 21) | (bi << 16) | (xo << 1); };

    const uint32_t bos[] = { 0, 4, 12, 16, 20, 24 };
    const uint32_t bis[] = { 0, 2, 4, 8, 20, 31 };

    for (uint32_t bo : bos)
    {
        for (uint32_t bi : bis)
        {
            cases.push_back({ "bc",    bc(bo, bi, 8) });
            cases.push_back({ "bcl",   bc(bo, bi, 8) | 1 });
            cases.push_back({ "bclr",  bbx(bo, bi, 16) });
            cases.push_back({ "bclrl", bbx(bo, bi, 16) | 1 });
            cases.push_back({ "bcctr", bbx(bo, bi, 528) });
            cases.push_back({ "bcctrl", bbx(bo, bi, 528) | 1 });
        }
    }
    cases.push_back({ "b",  b(8) });
    cases.push_back({ "bl", b(8) | 1 });

    int failures = 0;
    for (const Case& c : cases)
    {
        for (uint32_t lrOff : { 4u, 8u, 20u })
        {
            *(uint32_t*)&ram[codePa] = _BYTESWAP_UINT32(c.word);

            auto init = [&]()
            {
                Core->Reset();
                Core->cache->Enable(true); Core->icache->Enable(true);
                for (int i = 0; i < 16; i++) Core->regs.sr[i] = 0x80000000;
                Core->regs.spr[(int)Gekko::SPR::DBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::DBAT0L] = 0x00000002;
                Core->regs.spr[(int)Gekko::SPR::IBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::IBAT0L] = 0x00000002;
                Core->regs.msr = MSR_IR | MSR_DR | MSR_FP;
                Core->regs.cr = 0xA5A5A5A5;
                Core->regs.spr[(int)Gekko::SPR::XER] = 0;
                Core->regs.spr[(int)Gekko::SPR::LR] = effPc + lrOff;
                Core->regs.spr[(int)Gekko::SPR::CTR] = effPc + lrOff;
                Core->regs.pc = effPc;
            };

            init();
            Core->Step();
            uint32_t ipc = Core->regs.pc, ilr = Core->regs.spr[(int)Gekko::SPR::LR], ictr = Core->regs.spr[(int)Gekko::SPR::CTR], icr = Core->regs.cr;

            init();
#if defined(BENCH_WITH_JIT)
            Core->jit->Run();
#else
            Core->Step();
#endif
            uint32_t jpc = Core->regs.pc, jlr = Core->regs.spr[(int)Gekko::SPR::LR], jctr = Core->regs.spr[(int)Gekko::SPR::CTR], jcr = Core->regs.cr;

            if (ipc != jpc || ilr != jlr || ictr != jctr || icr != jcr)
            {
                if (failures < 10)
                    printf("  BRANCH FAIL %-6s word=%08X lrOff=%u: interp pc=%08X lr=%08X ctr=%08X cr=%08X | jit pc=%08X lr=%08X ctr=%08X cr=%08X\n",
                        c.name, c.word, lrOff, ipc, ilr, ictr, icr, jpc, jlr, jctr, jcr);
                failures++;
            }
        }
    }

    printf("branch test: %zu cases, %d failures\n", cases.size() * 3, failures);
    return failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Targeted Paired-Single differential test.
//
// For every translated form, run that instruction over the operand bit patterns
// that actually break floating point - NaN, infinity, denormal, both zeroes - on
// both engines and compare the whole register file.
//
// The random fuzzer covers PS only sparsely (a random word is a PS instruction
// rarely, and it needs the right operand pattern on top of that), which is how a
// NaN of the wrong sign survived 800 fuzzer programs and showed up only in a
// PS-heavy workload.

static uint32_t PsWord(uint32_t xo, uint32_t d, uint32_t a, uint32_t b, uint32_t c, uint32_t rc = 0)
{
    return (4u << 26) | (d << 21) | (a << 16) | (b << 11) | (c << 6) | (xo << 1) | rc;
}

static bool IsNanBits(uint64_t v)
{
    return (v & 0x7ff0'0000'0000'0000ull) == 0x7ff0'0000'0000'0000ull &&
           (v & 0x000f'ffff'ffff'ffffull) != 0;
}

static int RunPsTest(uint8_t* ram)
{
    struct PsCase { const char* name; uint32_t word; };

    // frD = f1, frA = f2, frB = f3, frC = f4, matching the field layout of the
    // two, three and one operand forms (see gen_workload.py).
    const uint32_t d = 1, a = 2, b = 3, c = 4;
    std::vector<PsCase> cases;
    auto add = [&](const char* n, uint32_t xo, int arity, bool rc)
    {
        uint32_t w = 0;
        switch (arity)
        {
        case 1:  w = PsWord(xo, d, 0, b, 0, rc); break;			// single operand: frB
        case 2:  w = PsWord(xo, d, a, b, 0, rc); break;			// frD = frA op frB
        case 3:  w = PsWord(xo, d, a, b, c, rc); break;			// frD = frA * frC + frB
        default: w = PsWord(xo, d, a, 0, c, rc); break;			// frD = frA op frC
        }
        cases.push_back({ n, w });
    };

    //       name          xo   arity rc
    add("ps_add",        21,  2, false);
    add("ps_add_d",      21,  2, true);
    add("ps_sub",        20,  2, false);
    add("ps_sub_d",      20,  2, true);
    add("ps_mul",        25,  4, false);
    add("ps_mul_d",      25,  4, true);
    add("ps_div",        18,  2, false);
    add("ps_div_d",      18,  2, true);
    add("ps_res",        24,  1, false);
    add("ps_res_d",      24,  1, true);
    add("ps_rsqrte",     26,  1, false);
    add("ps_rsqrte_d",   26,  1, true);
    add("ps_madd",       29,  3, false);
    add("ps_madd_d",     29,  3, true);
    add("ps_muls0",      12,  4, false);
    add("ps_muls0_d",    12,  4, true);
    add("ps_muls1",      13,  4, false);
    add("ps_muls1_d",    13,  4, true);
    add("ps_madds0",     14,  3, false);
    add("ps_madds0_d",   14,  3, true);
    add("ps_madds1",     15,  3, false);
    add("ps_madds1_d",   15,  3, true);
    add("ps_sum0",       10,  3, false);
    add("ps_sum0_d",     10,  3, true);
    add("ps_sum1",       11,  3, false);
    add("ps_sum1_d",     11,  3, true);
    add("ps_sel",        23,  3, false);
    add("ps_sel_d",      23,  3, true);
    add("ps_mr",         72,  1, false);
    add("ps_mr_d",       72,  1, true);
    add("ps_neg",        40,  1, false);
    add("ps_neg_d",      40,  1, true);
    add("ps_abs",        264, 1, false);
    add("ps_abs_d",      264, 1, true);
    add("ps_nabs",       136, 1, false);
    add("ps_nabs_d",     136, 1, true);
    add("ps_merge00",    528, 2, false);
    add("ps_merge00_d",  528, 2, true);
    add("ps_merge01",    560, 2, false);
    add("ps_merge10",    592, 2, false);
    add("ps_merge11",    624, 2, false);

    static const uint64_t patterns[] =
    {
        0x0000'0000'0000'0000ull,		// +0
        0x8000'0000'0000'0000ull,		// -0
        0x3ff0'0000'0000'0000ull,		// 1.0
        0xbff0'0000'0000'0000ull,		// -1.0
        0x4000'0000'0000'0000ull,		// 2.0
        0x7ff0'0000'0000'0000ull,		// +inf
        0xfff0'0000'0000'0000ull,		// -inf
        0x7ff8'0000'0000'0000ull,		// quiet NaN
        0xfff8'0000'0000'0000ull,		// -quiet NaN
        0x7ff0'0000'0000'0001ull,		// signaling NaN
        0x0000'0000'0000'0001ull,		// denormal
    };
    const int np = (int)(sizeof(patterns) / sizeof(patterns[0]));

    const uint32_t codePa = 0x200000;
    const uint32_t effPc = 0x80200000;

    auto setup = [&](const PsCase& tc, int i, int j, int k)
    {
        for (uint32_t pa = 0; pa < 0x1000; pa += 4)
            *(uint32_t*)&ram[codePa + pa] = _BYTESWAP_UINT32(0x60000000);	// nops
        *(uint32_t*)&ram[codePa] = _BYTESWAP_UINT32(tc.word);

        Core->Reset();
        Core->cache->Enable(true);
        Core->icache->Enable(true);
        for (int s = 0; s < 16; s++) Core->regs.sr[s] = 0x80000000;
        Core->regs.spr[(int)Gekko::SPR::DBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::DBAT0L] = 0x00000002;
        Core->regs.spr[(int)Gekko::SPR::IBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::IBAT0L] = 0x00000002;
        Core->regs.msr = MSR_IR | MSR_DR | MSR_FP;

        Core->regs.fpr[a].uval = patterns[i];
        Core->regs.ps1[a].uval = patterns[j];
        Core->regs.fpr[b].uval = patterns[j];
        Core->regs.ps1[b].uval = patterns[k];
        Core->regs.fpr[c].uval = patterns[k];
        Core->regs.ps1[c].uval = patterns[i];
        Core->regs.fpr[d].uval = 0x5555'5555'5555'5555ull;
        Core->regs.ps1[d].uval = 0xaaaa'aaaa'aaaa'aaaaull;
        Core->regs.fpscr = 0x1234'5678;
        Core->regs.cr = 0x9abc'def0;
        Core->regs.pc = effPc;
    };

    int failures = 0;
    uint64_t tested = 0;
    uint64_t nanCases = 0;
    std::vector<int> formFailures(cases.size(), 0);

    for (size_t ci = 0; ci < cases.size(); ci++)
    {
        const PsCase& tc = cases[ci];
        for (int i = 0; i < np; i++)
        for (int j = 0; j < np; j++)
        for (int k = 0; k < np; k++)
        {
            setup(tc, i, j, k);
            Core->Step();
            uint64_t if0 = Core->regs.fpr[d].uval, if1 = Core->regs.ps1[d].uval;
            uint32_t icr = Core->regs.cr, ifpscr = Core->regs.fpscr;

            setup(tc, i, j, k);
#if defined(BENCH_WITH_JIT)
            Core->jit->Run();
#else
            Core->Step();
#endif
            uint64_t jf0 = Core->regs.fpr[d].uval, jf1 = Core->regs.ps1[d].uval;
            tested++;

            // Payload and sign of a NaN result are not defined by C++: when both
            // operands of an operation are NaN, which one gets propagated is
            // decided by how the host compiler happened to allocate registers for
            // the interpreter's expression - GCC propagates the second operand in
            // ps_muls0 and the first one in ps_mul, in the same build. The
            // translation uses the order written in the source, so those cases can
            // disagree; they are counted here instead of failing silently. A NaN
            // against a number is still a failure.
            bool nanOnly = (if0 != jf0 || if1 != jf1) &&
                (if0 == jf0 || (IsNanBits(if0) && IsNanBits(jf0))) &&
                (if1 == jf1 || (IsNanBits(if1) && IsNanBits(jf1)));

            if (if0 != jf0 || if1 != jf1 || icr != Core->regs.cr || ifpscr != Core->regs.fpscr)
            {
                if (nanOnly && icr == Core->regs.cr && ifpscr == Core->regs.fpscr)
                {
                    nanCases++;
                    continue;
                }

                if (failures < 10)
                    printf("PS FAIL %-12s a=%016llX/%016llX b=%016llX/%016llX c=%016llX/%016llX:"
                        " ps0 %016llX vs %016llX, ps1 %016llX vs %016llX, cr %08X vs %08X\n",
                        tc.name,
                        (unsigned long long)patterns[i], (unsigned long long)patterns[j],
                        (unsigned long long)patterns[j], (unsigned long long)patterns[k],
                        (unsigned long long)patterns[k], (unsigned long long)patterns[i],
                        (unsigned long long)if0, (unsigned long long)jf0,
                        (unsigned long long)if1, (unsigned long long)jf1,
                        icr, Core->regs.cr);
                failures++;
                formFailures[ci]++;
            }
        }
    }

    for (size_t ci = 0; ci < cases.size(); ci++)
    {
        if (formFailures[ci])
            printf("  %-14s %d/%d cases diverge\n", cases[ci].name, formFailures[ci], np * np * np);
    }

    printf("ps test: %zu forms, %llu cases, %d failures, %llu NaN payload cases\n", cases.size(),
        (unsigned long long)tested, failures, (unsigned long long)nanCases);
    return failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
	const char* dolPath = (argc > 1) ? argv[1] : "pong.dol";
	uint64_t totalSteps = (argc > 2) ? strtoull(argv[2], nullptr, 0) : 50'000'000ull;
	uint64_t chunk = (argc > 3) ? strtoull(argv[3], nullptr, 0) : 1'000'000ull;

	const uint32_t ramSize = 24 * 1024 * 1024;
	uint8_t* ram = new uint8_t[ramSize];
	memset(ram, 0, ramSize);

	Flipper::ProcessorInterface::ram = ram;
	Flipper::ProcessorInterface::ramSize = ramSize;

	Core = new Gekko::GekkoCore();

	// The state that BootROM() leaves behind before the DOL is entered.
	for (int sr = 0; sr < 16; sr++) Core->regs.sr[sr] = 0x80000000;
	Core->regs.spr[(int)Gekko::SPR::DBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::DBAT0L] = 0x00000002;
	Core->regs.spr[(int)Gekko::SPR::DBAT1U] = 0xc0001fff; Core->regs.spr[(int)Gekko::SPR::DBAT1L] = 0x0000002a;
	Core->regs.spr[(int)Gekko::SPR::IBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::IBAT0L] = 0x00000002;
	Core->regs.msr |= (MSR_IR | MSR_DR);
	Core->regs.msr &= ~MSR_EE;
	Core->regs.msr |= MSR_FP;
	Core->regs.spr[(int)Gekko::SPR::PVR] = 0x00083214;

	// OS low-memory variables and the stack the IPL hands to the DOL.
	Core->WriteWord(0x80000028, ramSize);
	Core->WriteWord(0x8000002c, 1);
	Core->WriteWord(0x800000f0, ramSize);
	Core->WriteWord(0x800000f8, CPU_BUS_CLOCK);
	Core->WriteWord(0x800000fc, CPU_CORE_CLOCK);
	Core->WriteWord(0x80000034, 0x816ffffc - 0x10000);

	// default syscall handler
	{
		static const uint32_t default_syscall[] = { 0x2c01004c, 0xac04007c, 0x6400004c };
		uint32_t off = (uint32_t)Gekko::Exception::EXCEPTION_SYSTEM_CALL;
		for (int i = 0; i < 3; i++) *(uint32_t*)&ram[off + i * 4] = _BYTESWAP_UINT32(default_syscall[i]);
	}

	uint32_t entry = 0;
	if (const char* raw = getenv("BENCH_RAW"))
	{
		std::ifstream f(raw, std::ifstream::binary);
		if (!f.is_open()) { printf("Cannot open %s\n", raw); return 1; }
		f.read((char*)&ram[0x1000], 0x100000);
		entry = 0x80001000;
		// A game enables the caches early in OSInit; do the same so that the raw
		// workloads exercise the cached path rather than the PI fallback.
		Core->cache->Enable(true);
		Core->icache->Enable(true);
	}
	else if (!DOL::Load(dolPath, ram, ramSize, entry))
	{
		printf("Cannot load %s\n", dolPath);
		return 1;
	}

	Core->regs.gpr[1] = 0x816ffffc;
	Core->regs.gpr[13] = 0x81100000;
	Core->regs.pc = entry;

	if (getenv("BENCH_VALIDATE"))
	{
		// Decode the loaded program and report anything the decoder does not know.
		Gekko::DecoderInfo info{};
		int unknown = 0, total = 0;
		std::map<int, int> hist;
		for (uint32_t pa = 0x1000; pa < 0x1000 + 0x100000; pa += 4)
		{
			uint32_t w = _BYTESWAP_UINT32(*(uint32_t*)&ram[pa]);
			if (w == 0) continue;
			Gekko::Decoder::Decode(0x80000000 + pa, w, &info);
			total++;
			hist[(int)info.instr]++;
			if (info.instr == Gekko::Instruction::Unknown)
			{
				if (unknown < 10) printf("unknown opcode %08X at %08X\n", w, 0x80000000 + pa);
				unknown++;
			}
		}
		printf("validated %d words, %d unknown\n", total, unknown);
		// Print histogram using the disassembler names
		std::vector<std::pair<int,int>> v(hist.begin(), hist.end());
		std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; });
		for (auto& kv : v)
		{
			Gekko::DecoderInfo di{};
			di.instr = (Gekko::Instruction)kv.first;
			di.numParam = 0;
			printf("  %6d  %s\n", kv.second, Gekko::GekkoDisasm::InstrToString(&di).c_str());
		}
		return 0;
	}

	if (const char* d = getenv("BENCH_DISASM"))
	{
		uint32_t a = strtoul(d, nullptr, 0);
		uint32_t n = getenv("BENCH_DISASM_N") ? strtoul(getenv("BENCH_DISASM_N"), nullptr, 0) : 64;
		Gekko::DecoderInfo info{};
		for (uint32_t i = 0; i < n; i++)
		{
			uint32_t w = 0;
			Core->Fetch(a + i * 4, &w);
			printf("%08X  %08X  %s\n", a + i * 4, w, Gekko::GekkoDisasm::Disasm(a + i * 4, &info, false, true).c_str());
		}
		return 0;
	}

	printf("Loaded %s, entry %08X, running %llu instructions...\n", dolPath, entry, (unsigned long long)totalSteps);

	uint64_t trace = getenv("BENCH_TRACE") ? strtoull(getenv("BENCH_TRACE"), nullptr, 0) : 0;
	bool stats = getenv("BENCH_STATS") != nullptr;
	if (stats) Core->EnableOpcodeStats(true);
	bool prof = getenv("BENCH_PROF") != nullptr;
	if (prof) ProfStart();

	if (getenv("BENCH_PS_TEST"))
	{
		return RunPsTest(ram);
	}

	if (getenv("BENCH_BRANCH_TEST"))
	{
#if !defined(BENCH_WITH_JIT)
		printf("this build has no recompiler (reference build)\n");
		return 0;
#endif
		return BranchTest();
	}

	if (const char* ripl = getenv("BENCH_REAL_IPL"))
	{
		return RunRealIpl(ripl, getenv("BENCH_IPL_STEPS") ? strtoull(getenv("BENCH_IPL_STEPS"), nullptr, 0) : 20000000,
			JitRequested());
	}

	if (const char* ipl = getenv("BENCH_IPL"))
	{
		return RunIpl(ipl, getenv("BENCH_IPL_STEPS") ? strtoull(getenv("BENCH_IPL_STEPS"), nullptr, 0) : 2000000,
			JitRequested());
	}

	if (getenv("BENCH_MINI"))
	{
		// One instruction, executed by both engines from a fresh state.
		uint32_t w = strtoul(getenv("BENCH_MINI"), nullptr, 16);
		const uint32_t codePa = 0x200000, effPc = 0x80200000;
		*(uint32_t*)&ram[codePa] = _BYTESWAP_UINT32(w);
		*(uint32_t*)&ram[0x80010] = _BYTESWAP_UINT32(0x12345678);
		*(uint32_t*)&ram[0x80014] = _BYTESWAP_UINT32(0x9ABCDEF0);
		for (int pass = 0; pass < 2; pass++)
		{
			Core->Reset();
			Core->cache->Enable(true); Core->icache->Enable(true);
			for (int i = 0; i < 16; i++) Core->regs.sr[i] = 0x80000000;
			Core->regs.spr[(int)Gekko::SPR::DBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::DBAT0L] = 0x00000002;
			Core->regs.spr[(int)Gekko::SPR::IBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::IBAT0L] = 0x00000002;
			Core->regs.msr = MSR_IR | MSR_DR | MSR_FP;
			for (int g = 0; g < 32; g++) Core->regs.gpr[g] = 0x80080010;
			Core->regs.pc = effPc;

			printf("pass %d before: f13=%016llX gpr5=%08X gpr3=%08X\n", pass,
				(unsigned long long)Core->regs.fpr[13].uval, Core->regs.gpr[5], Core->regs.gpr[3]);
#if defined(BENCH_WITH_JIT)
			if (pass == 0) Core->jit->Run(); else Core->Step();
#else
			Core->Step();
#endif
			printf("pass %d after : f13=%016llX gpr5=%08X gpr3=%08X gpr6=%08X pc=%08X cr=%08X\n", pass,
				(unsigned long long)Core->regs.fpr[13].uval, Core->regs.gpr[5], Core->regs.gpr[3], Core->regs.gpr[6], Core->regs.pc, Core->regs.cr);
		}
		return 0;
	}

	if (const char* fz = getenv("BENCH_FUZZ"))
	{
		int iters = atoi(fz);
		int per = getenv("BENCH_FUZZ_N") ? atoi(getenv("BENCH_FUZZ_N")) : 64;
#if !defined(BENCH_WITH_JIT)
		printf("this build has no recompiler (reference build)\n");
		return 0;
#endif
		int fails = RunFuzzer(ram, iters, per);
		return fails == 0 ? 0 : 1;
	}

#if defined(BENCH_WITH_JIT)
	bool useJit = JitRequested();
#else
	bool useJit = false;
#endif
	bool stateTrace = getenv("BENCH_STATE_TRACE") != nullptr;

	uint64_t done = 0;
	auto t0 = std::chrono::high_resolution_clock::now();

	while (done < totalSteps)
	{
		uint64_t n = totalSteps - done;
		if (n > chunk) n = chunk;

		for (uint64_t i = 0; i < n; i++)
		{
			if (useJit && (uint64_t)Core->GetInstructionCounter() >= totalSteps)
			{
				break;
			}
#if defined(BENCH_WITH_JIT)
			if (useJit)
			{
				Core->jit->Run();
				if (stateTrace)
				{
					uint64_t h = 1469598103934665603ull;
					for (int g = 0; g < 32; g++) { h ^= Core->regs.gpr[g]; h *= 1099511628211ull; }
					printf("%08X %08X %016llX %08X %08X\n", Core->regs.pc, Core->regs.cr, (unsigned long long)h,
						Core->regs.spr[(int)Gekko::SPR::XER], (uint32_t)Core->regs.tb.uval);
				}
				continue;
			}
#endif
			if (trace && !stateTrace)
			{
				if (Core->regs.pc == 0x300)
				{
					printf("DSI at instruction %llu: DAR=%08X DSISR=%08X\n",
						(unsigned long long)(done + i),
						Core->regs.spr[(int)Gekko::SPR::DAR], Core->regs.spr[(int)Gekko::SPR::DSISR]);
					printf("  r27=%08X r26=%08X r28=%08X r31=%08X\n",
						Core->regs.gpr[27], Core->regs.gpr[26], Core->regs.gpr[28], Core->regs.gpr[31]);
					Core->regs.pc = 0;
					break;
				}
				if (done + i < trace) printf("%08X r27=%08X r31=%08X r28=%08X r30=%08X\n", Core->regs.pc, Core->regs.gpr[27], Core->regs.gpr[31], Core->regs.gpr[28], Core->regs.gpr[30]);
			}
			Core->Step();
			if (stateTrace)
			{
				uint64_t h = 1469598103934665603ull;
				for (int g = 0; g < 32; g++) { h ^= Core->regs.gpr[g]; h *= 1099511628211ull; }
				printf("%08X %08X %016llX %08X %08X\n", Core->regs.pc, Core->regs.cr, (unsigned long long)h,
					Core->regs.spr[(int)Gekko::SPR::XER], (uint32_t)Core->regs.tb.uval);
			}
		}
		done += n;

		uint32_t pc = Core->regs.pc;
		if (pc < 0x80000000 || pc >= 0x8C000000)
		{
			printf("PC left the game range at instruction %llu: %08X (stopping)\n",
				(unsigned long long)done, pc);
			break;
		}
	}

	if (stats) Core->EnableOpcodeStats(false);

	if (prof) ProfStop(getenv("BENCH_PROF"));

	auto t1 = std::chrono::high_resolution_clock::now();
	double sec = std::chrono::duration<double>(t1 - t0).count();

	if (useJit)
	{
		done = (uint64_t)Core->GetInstructionCounter();
	}

	uint64_t hash = HashState(Core, ram, ramSize);

	printf("Executed %llu instructions in %.3f s = %.2f MIPS\n",
		(unsigned long long)done, sec, done / sec / 1e6);
	printf("state hash: %016llX\n", (unsigned long long)hash);
	printf("MMIO reads %llu, writes %llu\n",
		(unsigned long long)Flipper::ProcessorInterface::mmioReads,
		(unsigned long long)Flipper::ProcessorInterface::mmioWrites);
	if (stats)
	{
		g_verbose = true;
		printf("--- opcode stats ---\n");
		Core->PrintOpcodeStats(25);
		g_verbose = false;
	}

	if (getenv("BENCH_REGS"))
	{
		printf("--- registers ---\n");
		for (int i = 0; i < 32; i++) printf("r%-2d = %08X%s", i, Core->regs.gpr[i], (i % 4 == 3) ? "\n" : "  ");
		printf("cr = %08X  msr = %08X\n", Core->regs.cr, Core->regs.msr);
		// A Paired-Single divergence hides here: the GPRs above, the pc and the
		// time base can all be identical while an FPR differs.
		for (int i = 0; i < 32; i++)
			printf("f%-2d = %016llX  ps1 = %016llX%s", i,
				(unsigned long long)Core->regs.fpr[i].uval,
				(unsigned long long)Core->regs.ps1[i].uval, (i % 2 == 1) ? "\n" : "  ");
		printf("fpscr = %08X\n", Core->regs.fpscr);
	}

	printf("pc=%08X lr=%08X ctr=%08X tb=%llu\n", Core->regs.pc,
		Core->regs.spr[(int)Gekko::SPR::LR], Core->regs.spr[(int)Gekko::SPR::CTR],
		(unsigned long long)Core->regs.tb.uval);

	return 0;
}
