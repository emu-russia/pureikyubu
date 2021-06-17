// Interpreter core API

#pragma once

namespace Gekko
{
	class Interpreter
	{
		friend Jitc;
		friend GekkoCoreUnitTest::GekkoCoreUnitTest;

		GekkoCore* core = nullptr;

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

		void fadd(AnalyzeInfo& info);
		void fadd_d(AnalyzeInfo& info);
		void fadds(AnalyzeInfo& info);
		void fadds_d(AnalyzeInfo& info);
		void fdiv(AnalyzeInfo& info);
		void fdiv_d(AnalyzeInfo& info);
		void fdivs(AnalyzeInfo& info);
		void fdivs_d(AnalyzeInfo& info);
		void fmul(AnalyzeInfo& info);
		void fmul_d(AnalyzeInfo& info);
		void fmuls(AnalyzeInfo& info);
		void fmuls_d(AnalyzeInfo& info);
		void fres(AnalyzeInfo& info);
		void fres_d(AnalyzeInfo& info);
		void frsqrte(AnalyzeInfo& info);
		void frsqrte_d(AnalyzeInfo& info);
		void fsub(AnalyzeInfo& info);
		void fsub_d(AnalyzeInfo& info);
		void fsubs(AnalyzeInfo& info);
		void fsubs_d(AnalyzeInfo& info);
		void fsel(AnalyzeInfo& info);
		void fsel_d(AnalyzeInfo& info);
		void fmadd(AnalyzeInfo& info);
		void fmadd_d(AnalyzeInfo& info);
		void fmadds(AnalyzeInfo& info);
		void fmadds_d(AnalyzeInfo& info);
		void fmsub(AnalyzeInfo& info);
		void fmsub_d(AnalyzeInfo& info);
		void fmsubs(AnalyzeInfo& info);
		void fmsubs_d(AnalyzeInfo& info);
		void fnmadd(AnalyzeInfo& info);
		void fnmadd_d(AnalyzeInfo& info);
		void fnmadds(AnalyzeInfo& info);
		void fnmadds_d(AnalyzeInfo& info);
		void fnmsub(AnalyzeInfo& info);
		void fnmsub_d(AnalyzeInfo& info);
		void fnmsubs(AnalyzeInfo& info);
		void fnmsubs_d(AnalyzeInfo& info);
		void fctiw(AnalyzeInfo& info);
		void fctiw_d(AnalyzeInfo& info);
		void fctiwz(AnalyzeInfo& info);
		void fctiwz_d(AnalyzeInfo& info);
		void frsp(AnalyzeInfo& info);
		void frsp_d(AnalyzeInfo& info);
		void fcmpo(AnalyzeInfo& info);
		void fcmpu(AnalyzeInfo& info);
		void fabs(AnalyzeInfo& info);
		void fabs_d(AnalyzeInfo& info);
		void fmr(AnalyzeInfo& info);
		void fmr_d(AnalyzeInfo& info);
		void fnabs(AnalyzeInfo& info);
		void fnabs_d(AnalyzeInfo& info);
		void fneg(AnalyzeInfo& info);
		void fneg_d(AnalyzeInfo& info);

		void mcrfs(AnalyzeInfo& info);
		void mffs(AnalyzeInfo& info);
		void mffs_d(AnalyzeInfo& info);
		void mtfsb0(AnalyzeInfo& info);
		void mtfsb0_d(AnalyzeInfo& info);
		void mtfsb1(AnalyzeInfo& info);
		void mtfsb1_d(AnalyzeInfo& info);
		void mtfsf(AnalyzeInfo& info);
		void mtfsf_d(AnalyzeInfo& info);
		void mtfsfi(AnalyzeInfo& info);
		void mtfsfi_d(AnalyzeInfo& info);

		void lfd(AnalyzeInfo& info);
		void lfdu(AnalyzeInfo& info);
		void lfdux(AnalyzeInfo& info);
		void lfdx(AnalyzeInfo& info);
		void lfs(AnalyzeInfo& info);
		void lfsu(AnalyzeInfo& info);
		void lfsux(AnalyzeInfo& info);
		void lfsx(AnalyzeInfo& info);
		void stfd(AnalyzeInfo& info);
		void stfdu(AnalyzeInfo& info);
		void stfdux(AnalyzeInfo& info);
		void stfdx(AnalyzeInfo& info);
		void stfiwx(AnalyzeInfo& info);
		void stfs(AnalyzeInfo& info);
		void stfsu(AnalyzeInfo& info);
		void stfsux(AnalyzeInfo& info);
		void stfsx(AnalyzeInfo& info);

		void add(AnalyzeInfo& info);
		void add_d(AnalyzeInfo& info);
		void addo(AnalyzeInfo& info);
		void addo_d(AnalyzeInfo& info);
		void addc(AnalyzeInfo& info);
		void addc_d(AnalyzeInfo& info);
		void addco(AnalyzeInfo& info);
		void addco_d(AnalyzeInfo& info);
		void adde(AnalyzeInfo& info);
		void adde_d(AnalyzeInfo& info);
		void addeo(AnalyzeInfo& info);
		void addeo_d(AnalyzeInfo& info);
		void addi(AnalyzeInfo& info);
		void addic(AnalyzeInfo& info);
		void addic_d(AnalyzeInfo& info);
		void addis(AnalyzeInfo& info);
		void addme(AnalyzeInfo& info);
		void addme_d(AnalyzeInfo& info);
		void addmeo(AnalyzeInfo& info);
		void addmeo_d(AnalyzeInfo& info);
		void addze(AnalyzeInfo& info);
		void addze_d(AnalyzeInfo& info);
		void addzeo(AnalyzeInfo& info);
		void addzeo_d(AnalyzeInfo& info);
		void divw(AnalyzeInfo& info);
		void divw_d(AnalyzeInfo& info);
		void divwo(AnalyzeInfo& info);
		void divwo_d(AnalyzeInfo& info);
		void divwu(AnalyzeInfo& info);
		void divwu_d(AnalyzeInfo& info);
		void divwuo(AnalyzeInfo& info);
		void divwuo_d(AnalyzeInfo& info);
		void mulhw(AnalyzeInfo& info);
		void mulhw_d(AnalyzeInfo& info);
		void mulhwu(AnalyzeInfo& info);
		void mulhwu_d(AnalyzeInfo& info);
		void mulli(AnalyzeInfo& info);
		void mullw(AnalyzeInfo& info);
		void mullw_d(AnalyzeInfo& info);
		void mullwo(AnalyzeInfo& info);
		void mullwo_d(AnalyzeInfo& info);
		void neg(AnalyzeInfo& info);
		void neg_d(AnalyzeInfo& info);
		void nego(AnalyzeInfo& info);
		void nego_d(AnalyzeInfo& info);
		void subf(AnalyzeInfo& info);
		void subf_d(AnalyzeInfo& info);
		void subfo(AnalyzeInfo& info);
		void subfo_d(AnalyzeInfo& info);
		void subfc(AnalyzeInfo& info);
		void subfc_d(AnalyzeInfo& info);
		void subfco(AnalyzeInfo& info);
		void subfco_d(AnalyzeInfo& info);
		void subfe(AnalyzeInfo& info);
		void subfe_d(AnalyzeInfo& info);
		void subfeo(AnalyzeInfo& info);
		void subfeo_d(AnalyzeInfo& info);
		void subfic(AnalyzeInfo& info);
		void subfme(AnalyzeInfo& info);
		void subfme_d(AnalyzeInfo& info);
		void subfmeo(AnalyzeInfo& info);
		void subfmeo_d(AnalyzeInfo& info);
		void subfze(AnalyzeInfo& info);
		void subfze_d(AnalyzeInfo& info);
		void subfzeo(AnalyzeInfo& info);
		void subfzeo_d(AnalyzeInfo& info);

		void lbz(AnalyzeInfo& info);
		void lbzu(AnalyzeInfo& info);
		void lbzux(AnalyzeInfo& info);
		void lbzx(AnalyzeInfo& info);
		void lha(AnalyzeInfo& info);
		void lhau(AnalyzeInfo& info);
		void lhaux(AnalyzeInfo& info);
		void lhax(AnalyzeInfo& info);
		void lhz(AnalyzeInfo& info);
		void lhzu(AnalyzeInfo& info);
		void lhzux(AnalyzeInfo& info);
		void lhzx(AnalyzeInfo& info);
		void lwz(AnalyzeInfo& info);
		void lwzu(AnalyzeInfo& info);
		void lwzux(AnalyzeInfo& info);
		void lwzx(AnalyzeInfo& info);
		void stb(AnalyzeInfo& info);
		void stbu(AnalyzeInfo& info);
		void stbux(AnalyzeInfo& info);
		void stbx(AnalyzeInfo& info);
		void sth(AnalyzeInfo& info);
		void sthu(AnalyzeInfo& info);
		void sthux(AnalyzeInfo& info);
		void sthx(AnalyzeInfo& info);
		void stw(AnalyzeInfo& info);
		void stwu(AnalyzeInfo& info);
		void stwux(AnalyzeInfo& info);
		void stwx(AnalyzeInfo& info);
		void lhbrx(AnalyzeInfo& info);
		void lwbrx(AnalyzeInfo& info);
		void sthbrx(AnalyzeInfo& info);
		void stwbrx(AnalyzeInfo& info);
		void lmw(AnalyzeInfo& info);
		void stmw(AnalyzeInfo& info);
		void lswi(AnalyzeInfo& info);
		void lswx(AnalyzeInfo& info);
		void stswi(AnalyzeInfo& info);
		void stswx(AnalyzeInfo& info);

		void _and(AnalyzeInfo& info);
		void and_d(AnalyzeInfo& info);
		void andc(AnalyzeInfo& info);
		void andc_d(AnalyzeInfo& info);
		void andi_d(AnalyzeInfo& info);
		void andis_d(AnalyzeInfo& info);
		void cntlzw(AnalyzeInfo& info);
		void cntlzw_d(AnalyzeInfo& info);
		void eqv(AnalyzeInfo& info);
		void eqv_d(AnalyzeInfo& info);
		void extsb(AnalyzeInfo& info);
		void extsb_d(AnalyzeInfo& info);
		void extsh(AnalyzeInfo& info);
		void extsh_d(AnalyzeInfo& info);
		void nand(AnalyzeInfo& info);
		void nand_d(AnalyzeInfo& info);
		void nor(AnalyzeInfo& info);
		void nor_d(AnalyzeInfo& info);
		void _or(AnalyzeInfo& info);
		void or_d(AnalyzeInfo& info);
		void orc(AnalyzeInfo& info);
		void orc_d(AnalyzeInfo& info);
		void ori(AnalyzeInfo& info);
		void oris(AnalyzeInfo& info);
		void _xor(AnalyzeInfo& info);
		void xor_d(AnalyzeInfo& info);
		void xori(AnalyzeInfo& info);
		void xoris(AnalyzeInfo& info);

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

		void psq_lx(AnalyzeInfo& info);
		void psq_stx(AnalyzeInfo& info);
		void psq_lux(AnalyzeInfo& info);
		void psq_stux(AnalyzeInfo& info);
		void psq_l(AnalyzeInfo& info);
		void psq_lu(AnalyzeInfo& info);
		void psq_st(AnalyzeInfo& info);
		void psq_stu(AnalyzeInfo& info);

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

		void eieio(AnalyzeInfo& info);
		void isync(AnalyzeInfo& info);
		void lwarx(AnalyzeInfo& info);
		void stwcx_d(AnalyzeInfo& info);
		void sync(AnalyzeInfo& info);
		void rfi(AnalyzeInfo& info);
		void sc(AnalyzeInfo& info);
		void tw(AnalyzeInfo& info);
		void twi(AnalyzeInfo& info);
		void mcrxr(AnalyzeInfo& info);
		void mfcr(AnalyzeInfo& info);
		void mfmsr(AnalyzeInfo& info);
		void mfspr(AnalyzeInfo& info);
		void mftb(AnalyzeInfo& info);
		void mtcrf(AnalyzeInfo& info);
		void mtmsr(AnalyzeInfo& info);
		void mtspr(AnalyzeInfo& info);
		void dcbf(AnalyzeInfo& info);
		void dcbi(AnalyzeInfo& info);
		void dcbst(AnalyzeInfo& info);
		void dcbt(AnalyzeInfo& info);
		void dcbtst(AnalyzeInfo& info);
		void dcbz(AnalyzeInfo& info);
		void dcbz_l(AnalyzeInfo& info);
		void icbi(AnalyzeInfo& info);
		void mfsr(AnalyzeInfo& info);
		void mfsrin(AnalyzeInfo& info);
		void mtsr(AnalyzeInfo& info);
		void mtsrin(AnalyzeInfo& info);
		void tlbie(AnalyzeInfo& info);
		void tlbsync(AnalyzeInfo& info);
		void eciwx(AnalyzeInfo& info);
		void ecowx(AnalyzeInfo& info);

		void callvm(AnalyzeInfo& info);

		uint32_t    rotmask[32][32];    // mask for integer rotate opcodes 
		bool        RESERVE = false;    // for lwarx/stwcx.   
		uint32_t    RESERVE_ADDR = 0;	// for lwarx/stwcx.
		float       ldScale[64];        // for paired-single loads
		float       stScale[64];        // for paired-single stores

		float dequantize(uint32_t data, GEKKO_QUANT_TYPE type, uint8_t scale);
		uint32_t quantize(float data, GEKKO_QUANT_TYPE type, uint8_t scale);

		void BranchCheck();
		bool BcTest(AnalyzeInfo& info);
		bool BctrTest(AnalyzeInfo& info);

		template <typename T>
		inline void CmpCommon(int crfd, T a, T b);

		void Dispatch(AnalyzeInfo& info);

	public:
		Interpreter(GekkoCore* _core);
		~Interpreter() {}

		void ExecuteOpcode();
		bool ExecuteInterpeterFallback();

		uint32_t GetRotMask(int mb, int me);
	};

}
