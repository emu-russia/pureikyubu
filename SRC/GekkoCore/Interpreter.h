// Interpreter core API

#pragma once

namespace Gekko
{
	class Interpreter
	{
		friend Jitc;
		friend GekkoCoreUnitTest::GekkoCoreUnitTest;

		GekkoCore* core = nullptr;

		void b(DecoderInfo& info);
		void ba(DecoderInfo& info);
		void bl(DecoderInfo& info);
		void bla(DecoderInfo& info);
		void bc(DecoderInfo& info);
		void bca(DecoderInfo& info);
		void bcl(DecoderInfo& info);
		void bcla(DecoderInfo& info);
		void bcctr(DecoderInfo& info);
		void bcctrl(DecoderInfo& info);
		void bclr(DecoderInfo& info);
		void bclrl(DecoderInfo& info);

		void cmpi(DecoderInfo& info);
		void cmp(DecoderInfo& info);
		void cmpli(DecoderInfo& info);
		void cmpl(DecoderInfo& info);

		void crand(DecoderInfo& info);
		void crandc(DecoderInfo& info);
		void creqv(DecoderInfo& info);
		void crnand(DecoderInfo& info);
		void crnor(DecoderInfo& info);
		void cror(DecoderInfo& info);
		void crorc(DecoderInfo& info);
		void crxor(DecoderInfo& info);
		void mcrf(DecoderInfo& info);

		void fadd(DecoderInfo& info);
		void fadd_d(DecoderInfo& info);
		void fadds(DecoderInfo& info);
		void fadds_d(DecoderInfo& info);
		void fdiv(DecoderInfo& info);
		void fdiv_d(DecoderInfo& info);
		void fdivs(DecoderInfo& info);
		void fdivs_d(DecoderInfo& info);
		void fmul(DecoderInfo& info);
		void fmul_d(DecoderInfo& info);
		void fmuls(DecoderInfo& info);
		void fmuls_d(DecoderInfo& info);
		void fres(DecoderInfo& info);
		void fres_d(DecoderInfo& info);
		void frsqrte(DecoderInfo& info);
		void frsqrte_d(DecoderInfo& info);
		void fsub(DecoderInfo& info);
		void fsub_d(DecoderInfo& info);
		void fsubs(DecoderInfo& info);
		void fsubs_d(DecoderInfo& info);
		void fsel(DecoderInfo& info);
		void fsel_d(DecoderInfo& info);
		void fmadd(DecoderInfo& info);
		void fmadd_d(DecoderInfo& info);
		void fmadds(DecoderInfo& info);
		void fmadds_d(DecoderInfo& info);
		void fmsub(DecoderInfo& info);
		void fmsub_d(DecoderInfo& info);
		void fmsubs(DecoderInfo& info);
		void fmsubs_d(DecoderInfo& info);
		void fnmadd(DecoderInfo& info);
		void fnmadd_d(DecoderInfo& info);
		void fnmadds(DecoderInfo& info);
		void fnmadds_d(DecoderInfo& info);
		void fnmsub(DecoderInfo& info);
		void fnmsub_d(DecoderInfo& info);
		void fnmsubs(DecoderInfo& info);
		void fnmsubs_d(DecoderInfo& info);
		void fctiw(DecoderInfo& info);
		void fctiw_d(DecoderInfo& info);
		void fctiwz(DecoderInfo& info);
		void fctiwz_d(DecoderInfo& info);
		void frsp(DecoderInfo& info);
		void frsp_d(DecoderInfo& info);
		void fcmpo(DecoderInfo& info);
		void fcmpu(DecoderInfo& info);
		void fabs(DecoderInfo& info);
		void fabs_d(DecoderInfo& info);
		void fmr(DecoderInfo& info);
		void fmr_d(DecoderInfo& info);
		void fnabs(DecoderInfo& info);
		void fnabs_d(DecoderInfo& info);
		void fneg(DecoderInfo& info);
		void fneg_d(DecoderInfo& info);

		void mcrfs(DecoderInfo& info);
		void mffs(DecoderInfo& info);
		void mffs_d(DecoderInfo& info);
		void mtfsb0(DecoderInfo& info);
		void mtfsb0_d(DecoderInfo& info);
		void mtfsb1(DecoderInfo& info);
		void mtfsb1_d(DecoderInfo& info);
		void mtfsf(DecoderInfo& info);
		void mtfsf_d(DecoderInfo& info);
		void mtfsfi(DecoderInfo& info);
		void mtfsfi_d(DecoderInfo& info);

		void lfd(DecoderInfo& info);
		void lfdu(DecoderInfo& info);
		void lfdux(DecoderInfo& info);
		void lfdx(DecoderInfo& info);
		void lfs(DecoderInfo& info);
		void lfsu(DecoderInfo& info);
		void lfsux(DecoderInfo& info);
		void lfsx(DecoderInfo& info);
		void stfd(DecoderInfo& info);
		void stfdu(DecoderInfo& info);
		void stfdux(DecoderInfo& info);
		void stfdx(DecoderInfo& info);
		void stfiwx(DecoderInfo& info);
		void stfs(DecoderInfo& info);
		void stfsu(DecoderInfo& info);
		void stfsux(DecoderInfo& info);
		void stfsx(DecoderInfo& info);

		void add(DecoderInfo& info);
		void add_d(DecoderInfo& info);
		void addo(DecoderInfo& info);
		void addo_d(DecoderInfo& info);
		void addc(DecoderInfo& info);
		void addc_d(DecoderInfo& info);
		void addco(DecoderInfo& info);
		void addco_d(DecoderInfo& info);
		void adde(DecoderInfo& info);
		void adde_d(DecoderInfo& info);
		void addeo(DecoderInfo& info);
		void addeo_d(DecoderInfo& info);
		void addi(DecoderInfo& info);
		void addic(DecoderInfo& info);
		void addic_d(DecoderInfo& info);
		void addis(DecoderInfo& info);
		void addme(DecoderInfo& info);
		void addme_d(DecoderInfo& info);
		void addmeo(DecoderInfo& info);
		void addmeo_d(DecoderInfo& info);
		void addze(DecoderInfo& info);
		void addze_d(DecoderInfo& info);
		void addzeo(DecoderInfo& info);
		void addzeo_d(DecoderInfo& info);
		void divw(DecoderInfo& info);
		void divw_d(DecoderInfo& info);
		void divwo(DecoderInfo& info);
		void divwo_d(DecoderInfo& info);
		void divwu(DecoderInfo& info);
		void divwu_d(DecoderInfo& info);
		void divwuo(DecoderInfo& info);
		void divwuo_d(DecoderInfo& info);
		void mulhw(DecoderInfo& info);
		void mulhw_d(DecoderInfo& info);
		void mulhwu(DecoderInfo& info);
		void mulhwu_d(DecoderInfo& info);
		void mulli(DecoderInfo& info);
		void mullw(DecoderInfo& info);
		void mullw_d(DecoderInfo& info);
		void mullwo(DecoderInfo& info);
		void mullwo_d(DecoderInfo& info);
		void neg(DecoderInfo& info);
		void neg_d(DecoderInfo& info);
		void nego(DecoderInfo& info);
		void nego_d(DecoderInfo& info);
		void subf(DecoderInfo& info);
		void subf_d(DecoderInfo& info);
		void subfo(DecoderInfo& info);
		void subfo_d(DecoderInfo& info);
		void subfc(DecoderInfo& info);
		void subfc_d(DecoderInfo& info);
		void subfco(DecoderInfo& info);
		void subfco_d(DecoderInfo& info);
		void subfe(DecoderInfo& info);
		void subfe_d(DecoderInfo& info);
		void subfeo(DecoderInfo& info);
		void subfeo_d(DecoderInfo& info);
		void subfic(DecoderInfo& info);
		void subfme(DecoderInfo& info);
		void subfme_d(DecoderInfo& info);
		void subfmeo(DecoderInfo& info);
		void subfmeo_d(DecoderInfo& info);
		void subfze(DecoderInfo& info);
		void subfze_d(DecoderInfo& info);
		void subfzeo(DecoderInfo& info);
		void subfzeo_d(DecoderInfo& info);

		void lbz(DecoderInfo& info);
		void lbzu(DecoderInfo& info);
		void lbzux(DecoderInfo& info);
		void lbzx(DecoderInfo& info);
		void lha(DecoderInfo& info);
		void lhau(DecoderInfo& info);
		void lhaux(DecoderInfo& info);
		void lhax(DecoderInfo& info);
		void lhz(DecoderInfo& info);
		void lhzu(DecoderInfo& info);
		void lhzux(DecoderInfo& info);
		void lhzx(DecoderInfo& info);
		void lwz(DecoderInfo& info);
		void lwzu(DecoderInfo& info);
		void lwzux(DecoderInfo& info);
		void lwzx(DecoderInfo& info);
		void stb(DecoderInfo& info);
		void stbu(DecoderInfo& info);
		void stbux(DecoderInfo& info);
		void stbx(DecoderInfo& info);
		void sth(DecoderInfo& info);
		void sthu(DecoderInfo& info);
		void sthux(DecoderInfo& info);
		void sthx(DecoderInfo& info);
		void stw(DecoderInfo& info);
		void stwu(DecoderInfo& info);
		void stwux(DecoderInfo& info);
		void stwx(DecoderInfo& info);
		void lhbrx(DecoderInfo& info);
		void lwbrx(DecoderInfo& info);
		void sthbrx(DecoderInfo& info);
		void stwbrx(DecoderInfo& info);
		void lmw(DecoderInfo& info);
		void stmw(DecoderInfo& info);
		void lswi(DecoderInfo& info);
		void lswx(DecoderInfo& info);
		void stswi(DecoderInfo& info);
		void stswx(DecoderInfo& info);

		void _and(DecoderInfo& info);
		void and_d(DecoderInfo& info);
		void andc(DecoderInfo& info);
		void andc_d(DecoderInfo& info);
		void andi_d(DecoderInfo& info);
		void andis_d(DecoderInfo& info);
		void cntlzw(DecoderInfo& info);
		void cntlzw_d(DecoderInfo& info);
		void eqv(DecoderInfo& info);
		void eqv_d(DecoderInfo& info);
		void extsb(DecoderInfo& info);
		void extsb_d(DecoderInfo& info);
		void extsh(DecoderInfo& info);
		void extsh_d(DecoderInfo& info);
		void nand(DecoderInfo& info);
		void nand_d(DecoderInfo& info);
		void nor(DecoderInfo& info);
		void nor_d(DecoderInfo& info);
		void _or(DecoderInfo& info);
		void or_d(DecoderInfo& info);
		void orc(DecoderInfo& info);
		void orc_d(DecoderInfo& info);
		void ori(DecoderInfo& info);
		void oris(DecoderInfo& info);
		void _xor(DecoderInfo& info);
		void xor_d(DecoderInfo& info);
		void xori(DecoderInfo& info);
		void xoris(DecoderInfo& info);

		void ps_div(DecoderInfo& info);
		void ps_div_d(DecoderInfo& info);
		void ps_sub(DecoderInfo& info);
		void ps_sub_d(DecoderInfo& info);
		void ps_add(DecoderInfo& info);
		void ps_add_d(DecoderInfo& info);
		void ps_sel(DecoderInfo& info);
		void ps_sel_d(DecoderInfo& info);
		void ps_res(DecoderInfo& info);
		void ps_res_d(DecoderInfo& info);
		void ps_mul(DecoderInfo& info);
		void ps_mul_d(DecoderInfo& info);
		void ps_rsqrte(DecoderInfo& info);
		void ps_rsqrte_d(DecoderInfo& info);
		void ps_msub(DecoderInfo& info);
		void ps_msub_d(DecoderInfo& info);
		void ps_madd(DecoderInfo& info);
		void ps_madd_d(DecoderInfo& info);
		void ps_nmsub(DecoderInfo& info);
		void ps_nmsub_d(DecoderInfo& info);
		void ps_nmadd(DecoderInfo& info);
		void ps_nmadd_d(DecoderInfo& info);
		void ps_neg(DecoderInfo& info);
		void ps_neg_d(DecoderInfo& info);
		void ps_mr(DecoderInfo& info);
		void ps_mr_d(DecoderInfo& info);
		void ps_nabs(DecoderInfo& info);
		void ps_nabs_d(DecoderInfo& info);
		void ps_abs(DecoderInfo& info);
		void ps_abs_d(DecoderInfo& info);

		void ps_sum0(DecoderInfo& info);
		void ps_sum0_d(DecoderInfo& info);
		void ps_sum1(DecoderInfo& info);
		void ps_sum1_d(DecoderInfo& info);
		void ps_muls0(DecoderInfo& info);
		void ps_muls0_d(DecoderInfo& info);
		void ps_muls1(DecoderInfo& info);
		void ps_muls1_d(DecoderInfo& info);
		void ps_madds0(DecoderInfo& info);
		void ps_madds0_d(DecoderInfo& info);
		void ps_madds1(DecoderInfo& info);
		void ps_madds1_d(DecoderInfo& info);
		void ps_cmpu0(DecoderInfo& info);
		void ps_cmpo0(DecoderInfo& info);
		void ps_cmpu1(DecoderInfo& info);
		void ps_cmpo1(DecoderInfo& info);
		void ps_merge00(DecoderInfo& info);
		void ps_merge00_d(DecoderInfo& info);
		void ps_merge01(DecoderInfo& info);
		void ps_merge01_d(DecoderInfo& info);
		void ps_merge10(DecoderInfo& info);
		void ps_merge10_d(DecoderInfo& info);
		void ps_merge11(DecoderInfo& info);
		void ps_merge11_d(DecoderInfo& info);

		void psq_lx(DecoderInfo& info);
		void psq_stx(DecoderInfo& info);
		void psq_lux(DecoderInfo& info);
		void psq_stux(DecoderInfo& info);
		void psq_l(DecoderInfo& info);
		void psq_lu(DecoderInfo& info);
		void psq_st(DecoderInfo& info);
		void psq_stu(DecoderInfo& info);

		void rlwimi(DecoderInfo& info);
		void rlwimi_d(DecoderInfo& info);
		void rlwinm(DecoderInfo& info);
		void rlwinm_d(DecoderInfo& info);
		void rlwnm(DecoderInfo& info);
		void rlwnm_d(DecoderInfo& info);

		void slw(DecoderInfo& info);
		void slw_d(DecoderInfo& info);
		void sraw(DecoderInfo& info);
		void sraw_d(DecoderInfo& info);
		void srawi(DecoderInfo& info);
		void srawi_d(DecoderInfo& info);
		void srw(DecoderInfo& info);
		void srw_d(DecoderInfo& info);

		void eieio(DecoderInfo& info);
		void isync(DecoderInfo& info);
		void lwarx(DecoderInfo& info);
		void stwcx_d(DecoderInfo& info);
		void sync(DecoderInfo& info);
		void rfi(DecoderInfo& info);
		void sc(DecoderInfo& info);
		void tw(DecoderInfo& info);
		void twi(DecoderInfo& info);
		void mcrxr(DecoderInfo& info);
		void mfcr(DecoderInfo& info);
		void mfmsr(DecoderInfo& info);
		void mfspr(DecoderInfo& info);
		void mftb(DecoderInfo& info);
		void mtcrf(DecoderInfo& info);
		void mtmsr(DecoderInfo& info);
		void mtspr(DecoderInfo& info);
		void dcbf(DecoderInfo& info);
		void dcbi(DecoderInfo& info);
		void dcbst(DecoderInfo& info);
		void dcbt(DecoderInfo& info);
		void dcbtst(DecoderInfo& info);
		void dcbz(DecoderInfo& info);
		void dcbz_l(DecoderInfo& info);
		void icbi(DecoderInfo& info);
		void mfsr(DecoderInfo& info);
		void mfsrin(DecoderInfo& info);
		void mtsr(DecoderInfo& info);
		void mtsrin(DecoderInfo& info);
		void tlbie(DecoderInfo& info);
		void tlbsync(DecoderInfo& info);
		void eciwx(DecoderInfo& info);
		void ecowx(DecoderInfo& info);

		void callvm(DecoderInfo& info);

		uint32_t    rotmask[32][32];    // mask for integer rotate opcodes 
		float       ldScale[64];        // for paired-single loads
		float       stScale[64];        // for paired-single stores

		float dequantize(uint32_t data, GEKKO_QUANT_TYPE type, uint8_t scale);
		uint32_t quantize(float data, GEKKO_QUANT_TYPE type, uint8_t scale);

		void BranchCheck();
		bool BcTest(DecoderInfo& info);
		bool BctrTest(DecoderInfo& info);

		template <typename T>
		inline void CmpCommon(int crfd, T a, T b);

		void Dispatch(DecoderInfo& info);

		uint32_t CarryBit;
		uint32_t OverflowBit;

		uint32_t FullAdder(uint32_t a, uint32_t b);
		uint32_t Rotl32(int sa, uint32_t data);

		void SET_XER_SO() { core->regs.spr[Gekko::SPR::XER] |= GEKKO_XER_SO; }
		void SET_XER_OV() { core->regs.spr[Gekko::SPR::XER] |= GEKKO_XER_OV; }
		void SET_XER_CA() { core->regs.spr[Gekko::SPR::XER] |= GEKKO_XER_CA; }

		void RESET_XER_SO() { core->regs.spr[Gekko::SPR::XER] &= ~GEKKO_XER_SO; }
		void RESET_XER_OV() { core->regs.spr[Gekko::SPR::XER] &= ~GEKKO_XER_OV; }
		void RESET_XER_CA() { core->regs.spr[Gekko::SPR::XER] &= ~GEKKO_XER_CA; }

	public:
		Interpreter(GekkoCore* _core);
		~Interpreter() {}

		void ExecuteOpcode();
		bool ExecuteInterpeterFallback();

		uint32_t GetRotMask(int mb, int me);
	};

}
