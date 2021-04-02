
#pragma once

namespace Gekko
{
	// interpreter core API

	extern void (*bx[4])(uint32_t);
	extern void (*c_1[64])(uint32_t);
	extern void (*c_19[2048])(uint32_t);
	extern void (*c_31[2048])(uint32_t);
	extern void (*c_59[64])(uint32_t);
	extern void (*c_63[2048])(uint32_t);
	extern void (*c_4[2048])(uint32_t);

	class Interpreter
	{
		friend Jitc;

		GekkoCore* core = nullptr;

		// Instruction handlers are gradually being replaced by a new interpreter implementation  (#190)

		bool BcTest(AnalyzeInfo& info);
		bool BctrTest(AnalyzeInfo& info);
		void b(AnalyzeInfo& info);
		void ba(AnalyzeInfo& info);
		void bl(AnalyzeInfo& info);
		void bla(AnalyzeInfo& info);
		void bc(AnalyzeInfo& info);
		void bca(AnalyzeInfo& info);
		void bcl(AnalyzeInfo& info);
		void bcla(AnalyzeInfo& info);
		void bcctr(AnalyzeInfo& info);
		void bcctrl(AnalyzeInfo& info);
		void bclr(AnalyzeInfo& info);
		void bclrl(AnalyzeInfo& info);

		template <typename T>
		inline void CmpCommon(int crfd, T a, T b);
		void cmpi(AnalyzeInfo& info);
		void cmp(AnalyzeInfo& info);
		void cmpli(AnalyzeInfo& info);
		void cmpl(AnalyzeInfo& info);

		void crand(AnalyzeInfo& info);
		void crandc(AnalyzeInfo& info);
		void creqv(AnalyzeInfo& info);
		void crnand(AnalyzeInfo& info);
		void crnor(AnalyzeInfo& info);
		void cror(AnalyzeInfo& info);
		void crorc(AnalyzeInfo& info);
		void crxor(AnalyzeInfo& info);
		void mcrf(AnalyzeInfo& info);

		static void c_FADD(uint32_t op);
		static void c_FADDD(uint32_t op);
		static void c_FADDS(uint32_t op);
		static void c_FADDSD(uint32_t op);
		static void c_FSUB(uint32_t op);
		static void c_FSUBD(uint32_t op);
		static void c_FSUBS(uint32_t op);
		static void c_FSUBSD(uint32_t op);
		static void c_FMUL(uint32_t op);
		static void c_FMULD(uint32_t op);
		static void c_FMULS(uint32_t op);
		static void c_FMULSD(uint32_t op);
		static void c_FDIV(uint32_t op);
		static void c_FDIVD(uint32_t op);
		static void c_FDIVS(uint32_t op);
		static void c_FDIVSD(uint32_t op);
		static void c_FRES(uint32_t op);
		static void c_FRESD(uint32_t op);
		static void c_FRSQRTE(uint32_t op);
		static void c_FRSQRTED(uint32_t op);
		static void c_FSEL(uint32_t op);
		static void c_FSELD(uint32_t op);
		static void c_FMADD(uint32_t op);
		static void c_FMADDD(uint32_t op);
		static void c_FMADDS(uint32_t op);
		static void c_FMADDSD(uint32_t op);
		static void c_FMSUB(uint32_t op);
		static void c_FMSUBD(uint32_t op);
		static void c_FMSUBS(uint32_t op);
		static void c_FMSUBSD(uint32_t op);
		static void c_FNMADD(uint32_t op);
		static void c_FNMADDD(uint32_t op);
		static void c_FNMADDS(uint32_t op);
		static void c_FNMADDSD(uint32_t op);
		static void c_FNMSUB(uint32_t op);
		static void c_FNMSUBD(uint32_t op);
		static void c_FNMSUBS(uint32_t op);
		static void c_FNMSUBSD(uint32_t op);
		static void c_FRSP(uint32_t op);
		static void c_FRSPD(uint32_t op);
		static void c_FCTIW(uint32_t op);
		static void c_FCTIWD(uint32_t op);
		static void c_FCTIWZ(uint32_t op);
		static void c_FCTIWZD(uint32_t op);
		static void c_FNEG(uint32_t op);
		static void c_FNEGD(uint32_t op);
		static void c_FABS(uint32_t op);
		static void c_FABSD(uint32_t op);
		static void c_FNABS(uint32_t op);
		static void c_FNABSD(uint32_t op);
		static void c_FCMPU(uint32_t op);
		static void c_FCMPO(uint32_t op);
		static void c_MFFS(uint32_t op);
		static void c_MFFSD(uint32_t op);
		static void c_MCRFS(uint32_t op);
		static void c_MTFSFI(uint32_t op);
		static void c_MTFSFID(uint32_t op);
		static void c_MTFSF(uint32_t op);
		static void c_MTFSFD(uint32_t op);
		static void c_MTFSB0(uint32_t op);
		static void c_MTFSB0D(uint32_t op);
		static void c_MTFSB1(uint32_t op);
		static void c_MTFSB1D(uint32_t op);
		static void c_FMR(uint32_t op);
		static void c_FMRD(uint32_t op);

		static void c_LFS(uint32_t op);
		static void c_LFSX(uint32_t op);
		static void c_LFSU(uint32_t op);
		static void c_LFSUX(uint32_t op);
		static void c_LFD(uint32_t op);
		static void c_LFDX(uint32_t op);
		static void c_LFDU(uint32_t op);
		static void c_LFDUX(uint32_t op);
		static void c_STFS(uint32_t op);
		static void c_STFSX(uint32_t op);
		static void c_STFSU(uint32_t op);
		static void c_STFSUX(uint32_t op);
		static void c_STFD(uint32_t op);
		static void c_STFDX(uint32_t op);
		static void c_STFDU(uint32_t op);
		static void c_STFDUX(uint32_t op);
		static void c_STFIWX(uint32_t op);

		static void c_ADDI(uint32_t op);
		static void c_ADDIS(uint32_t op);
		static void c_ADD(uint32_t op);
		static void c_ADDD(uint32_t op);
		static void c_ADDO(uint32_t op);
		static void c_ADDOD(uint32_t op);
		static void c_SUBF(uint32_t op);
		static void c_SUBFD(uint32_t op);
		static void c_SUBFO(uint32_t op);
		static void c_SUBFOD(uint32_t op);
		static void c_ADDIC(uint32_t op);
		static void c_ADDICD(uint32_t op);
		static void c_SUBFIC(uint32_t op);
		static void c_ADDC(uint32_t op);
		static void c_ADDCD(uint32_t op);
		static void c_ADDCO(uint32_t op);
		static void c_ADDCOD(uint32_t op);
		static void c_SUBFC(uint32_t op);
		static void c_SUBFCD(uint32_t op);
		static void c_ADDE(uint32_t op);
		static void c_ADDED(uint32_t op);
		static void c_SUBFE(uint32_t op);
		static void c_SUBFED(uint32_t op);
		static void c_ADDME(uint32_t op);
		static void c_ADDMED(uint32_t op);
		static void c_ADDMEO(uint32_t op);
		static void c_ADDMEOD(uint32_t op);
		static void c_SUBFME(uint32_t op);
		static void c_SUBFMED(uint32_t op);
		static void c_SUBFMEO(uint32_t op);
		static void c_SUBFMEOD(uint32_t op);
		static void c_ADDZE(uint32_t op);
		static void c_ADDZED(uint32_t op);
		static void c_ADDZEO(uint32_t op);
		static void c_ADDZEOD(uint32_t op);
		static void c_SUBFZE(uint32_t op);
		static void c_SUBFZED(uint32_t op);
		static void c_SUBFZEO(uint32_t op);
		static void c_SUBFZEOD(uint32_t op);
		static void c_NEG(uint32_t op);
		static void c_NEGD(uint32_t op);
		static void c_MULLI(uint32_t op);
		static void c_MULLW(uint32_t op);
		static void c_MULLWD(uint32_t op);
		static void c_MULHW(uint32_t op);
		static void c_MULHWD(uint32_t op);
		static void c_MULHWU(uint32_t op);
		static void c_MULHWUD(uint32_t op);
		static void c_DIVW(uint32_t op);
		static void c_DIVWD(uint32_t op);
		static void c_DIVWU(uint32_t op);
		static void c_DIVWUD(uint32_t op);

		static void c_LBZ(uint32_t op);
		static void c_LBZX(uint32_t op);
		static void c_LBZU(uint32_t op);
		static void c_LBZUX(uint32_t op);
		static void c_LHZ(uint32_t op);
		static void c_LHZX(uint32_t op);
		static void c_LHZU(uint32_t op);
		static void c_LHZUX(uint32_t op);
		static void c_LHA(uint32_t op);
		static void c_LHAX(uint32_t op);
		static void c_LHAU(uint32_t op);
		static void c_LHAUX(uint32_t op);
		static void c_LWZ(uint32_t op);
		static void c_LWZX(uint32_t op);
		static void c_LWZU(uint32_t op);
		static void c_LWZUX(uint32_t op);
		static void c_STB(uint32_t op);
		static void c_STBX(uint32_t op);
		static void c_STBU(uint32_t op);
		static void c_STBUX(uint32_t op);
		static void c_STH(uint32_t op);
		static void c_STHX(uint32_t op);
		static void c_STHU(uint32_t op);
		static void c_STHUX(uint32_t op);
		static void c_STW(uint32_t op);
		static void c_STWX(uint32_t op);
		static void c_STWU(uint32_t op);
		static void c_STWUX(uint32_t op);
		static void c_LHBRX(uint32_t op);
		static void c_LWBRX(uint32_t op);
		static void c_STHBRX(uint32_t op);
		static void c_STWBRX(uint32_t op);
		static void c_LMW(uint32_t op);
		static void c_STMW(uint32_t op);
		static void c_LSWI(uint32_t op);
		static void c_STSWI(uint32_t op);
		static void c_LWARX(uint32_t op);
		static void c_STWCXD(uint32_t op);

		static void c_ANDID(uint32_t op);
		static void c_ANDISD(uint32_t op);
		static void c_ORI(uint32_t op);
		static void c_ORIS(uint32_t op);
		static void c_XORI(uint32_t op);
		static void c_XORIS(uint32_t op);
		static void c_AND(uint32_t op);
		static void c_ANDD(uint32_t op);
		static void c_OR(uint32_t op);
		static void c_ORD(uint32_t op);
		static void c_XOR(uint32_t op);
		static void c_XORD(uint32_t op);
		static void c_NAND(uint32_t op);
		static void c_NANDD(uint32_t op);
		static void c_NOR(uint32_t op);
		static void c_NORD(uint32_t op);
		static void c_EQV(uint32_t op);
		static void c_EQVD(uint32_t op);
		static void c_ANDC(uint32_t op);
		static void c_ANDCD(uint32_t op);
		static void c_ORC(uint32_t op);
		static void c_ORCD(uint32_t op);
		static void c_EXTSB(uint32_t op);
		static void c_EXTSBD(uint32_t op);
		static void c_EXTSH(uint32_t op);
		static void c_EXTSHD(uint32_t op);
		static void c_CNTLZW(uint32_t op);
		static void c_CNTLZWD(uint32_t op);

		void ps_div(AnalyzeInfo& info);
		void ps_div_d(AnalyzeInfo& info);
		void ps_sub(AnalyzeInfo& info);
		void ps_sub_d(AnalyzeInfo& info);
		void ps_add(AnalyzeInfo& info);
		void ps_add_d(AnalyzeInfo& info);
		void ps_sel(AnalyzeInfo& info);
		void ps_sel_d(AnalyzeInfo& info);
		void ps_res(AnalyzeInfo& info);
		void ps_res_d(AnalyzeInfo& info);
		void ps_mul(AnalyzeInfo& info);
		void ps_mul_d(AnalyzeInfo& info);
		void ps_rsqrte(AnalyzeInfo& info);
		void ps_rsqrte_d(AnalyzeInfo& info);
		void ps_msub(AnalyzeInfo& info);
		void ps_msub_d(AnalyzeInfo& info);
		void ps_madd(AnalyzeInfo& info);
		void ps_madd_d(AnalyzeInfo& info);
		void ps_nmsub(AnalyzeInfo& info);
		void ps_nmsub_d(AnalyzeInfo& info);
		void ps_nmadd(AnalyzeInfo& info);
		void ps_nmadd_d(AnalyzeInfo& info);
		void ps_neg(AnalyzeInfo& info);
		void ps_neg_d(AnalyzeInfo& info);
		void ps_mr(AnalyzeInfo& info);
		void ps_mr_d(AnalyzeInfo& info);
		void ps_nabs(AnalyzeInfo& info);
		void ps_nabs_d(AnalyzeInfo& info);
		void ps_abs(AnalyzeInfo& info);
		void ps_abs_d(AnalyzeInfo& info);

		void ps_sum0(AnalyzeInfo& info);
		void ps_sum0_d(AnalyzeInfo& info);
		void ps_sum1(AnalyzeInfo& info);
		void ps_sum1_d(AnalyzeInfo& info);
		void ps_muls0(AnalyzeInfo& info);
		void ps_muls0_d(AnalyzeInfo& info);
		void ps_muls1(AnalyzeInfo& info);
		void ps_muls1_d(AnalyzeInfo& info);
		void ps_madds0(AnalyzeInfo& info);
		void ps_madds0_d(AnalyzeInfo& info);
		void ps_madds1(AnalyzeInfo& info);
		void ps_madds1_d(AnalyzeInfo& info);
		void ps_cmpu0(AnalyzeInfo& info);
		void ps_cmpo0(AnalyzeInfo& info);
		void ps_cmpu1(AnalyzeInfo& info);
		void ps_cmpo1(AnalyzeInfo& info);
		void ps_merge00(AnalyzeInfo& info);
		void ps_merge00_d(AnalyzeInfo& info);
		void ps_merge01(AnalyzeInfo& info);
		void ps_merge01_d(AnalyzeInfo& info);
		void ps_merge10(AnalyzeInfo& info);
		void ps_merge10_d(AnalyzeInfo& info);
		void ps_merge11(AnalyzeInfo& info);
		void ps_merge11_d(AnalyzeInfo& info);

		static void c_PSQ_L(uint32_t op);
		static void c_PSQ_LX(uint32_t op);
		static void c_PSQ_LU(uint32_t op);
		static void c_PSQ_LUX(uint32_t op);
		static void c_PSQ_ST(uint32_t op);
		static void c_PSQ_STX(uint32_t op);
		static void c_PSQ_STU(uint32_t op);
		static void c_PSQ_STUX(uint32_t op);

		void rlwimi(AnalyzeInfo& info);
		void rlwimi_d(AnalyzeInfo& info);
		void rlwinm(AnalyzeInfo& info);
		void rlwinm_d(AnalyzeInfo& info);
		void rlwnm(AnalyzeInfo& info);
		void rlwnm_d(AnalyzeInfo& info);

		void slw(AnalyzeInfo& info);
		void slw_d(AnalyzeInfo& info);
		void sraw(AnalyzeInfo& info);
		void sraw_d(AnalyzeInfo& info);
		void srawi(AnalyzeInfo& info);
		void srawi_d(AnalyzeInfo& info);
		void srw(AnalyzeInfo& info);
		void srw_d(AnalyzeInfo& info);

		static void c_TWI(uint32_t op);
		static void c_TW(uint32_t op);
		static void c_SC(uint32_t op);
		static void c_RFI(uint32_t op);
		static void c_MTCRF(uint32_t op);
		static void c_MCRXR(uint32_t op);
		static void c_MFCR(uint32_t op);
		static void c_MTMSR(uint32_t op);
		static void c_MFMSR(uint32_t op);
		static void c_MTSPR(uint32_t op);
		static void c_MFSPR(uint32_t op);
		static void c_MFTB(uint32_t op);
		static void c_MTSR(uint32_t op);
		static void c_MTSRIN(uint32_t op);
		static void c_MFSR(uint32_t op);
		static void c_MFSRIN(uint32_t op);
		static void c_EIEIO(uint32_t op);
		static void c_SYNC(uint32_t op);
		static void c_ISYNC(uint32_t op);
		static void c_TLBSYNC(uint32_t op);
		static void c_TLBIE(uint32_t op);
		static void c_DCBT(uint32_t op);
		static void c_DCBTST(uint32_t op);
		static void c_DCBZ(uint32_t op);
		static void c_DCBZ_L(uint32_t op);
		static void c_DCBST(uint32_t op);
		static void c_DCBF(uint32_t op);
		static void c_DCBI(uint32_t op);
		static void c_ICBI(uint32_t op);

		static void c_NI(uint32_t op);
		static void c_HL(uint32_t op);

		static void c_OP19(uint32_t op);
		static void c_OP31(uint32_t op);
		static void c_OP59(uint32_t op);
		static void c_OP63(uint32_t op);
		static void c_OP4(uint32_t op);

		// setup extension tables 
		void InitTables();

		uint32_t    rotmask[32][32];    // mask for integer rotate opcodes 
		bool        RESERVE;            // for lwarx/stwcx.   
		uint32_t    RESERVE_ADDR;       // for lwarx/stwcx.
		float       ldScale[64];        // for paired-single loads
		float       stScale[64];        // for paired-single stores

		static float dequantize(uint32_t data, GEKKO_QUANT_TYPE type, uint8_t scale);
		static uint32_t quantize(float data, GEKKO_QUANT_TYPE type, uint8_t scale);

		void BranchCheck();

		void Dispatch(AnalyzeInfo& info, uint32_t instr /* eventually will be removed, used for fallback. */ );

	public:
		Interpreter(GekkoCore* _core)
		{
			core = _core;

			// setup interpreter tables
			InitTables();
		}
		~Interpreter()
		{
		}

		void ExecuteOpcode();
		void ExecuteOpcodeDirect(uint32_t pc, uint32_t instr);
		bool ExecuteInterpeterFallback();

		uint32_t GetRotMask(int mb, int me);
	};

}
