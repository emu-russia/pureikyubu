// Private interpreter stuff

#pragma once

#define IS_XER_SO       ((core->regs.spr[Gekko::SPR::XER] & GEKKO_XER_SO) != 0)
#define IS_XER_OV       ((core->regs.spr[Gekko::SPR::XER] & GEKKO_XER_OV) != 0)
#define IS_XER_CA       ((core->regs.spr[Gekko::SPR::XER] & GEKKO_XER_CA) != 0)

#define IS_NAN(n)       (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0)
#define IS_SNAN(n)      (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0 && ((n) & 0x0008000000000000) == 0)
#define SET_CRF(n, c)   (core->regs.cr = (core->regs.cr & (~(0xf0000000 >> (4 * n)))) | (c << (4 * (7 - n))))

#define COMPUTE_CR0(r)                                  \
{                                                       \
	(core->regs.cr = (core->regs.cr & 0x0fff'ffff)|     \
	(IS_XER_SO ? (GEKKO_CR0_SO) : (0)) |                \
	(((int32_t)(r) < 0) ? (GEKKO_CR0_LT) : (((int32_t)(r) > 0) ? (GEKKO_CR0_GT) : (GEKKO_CR0_EQ))));\
}

#define COMPUTE_CR1()                                   \
{                                                       \
	core->regs.cr = (core->regs.cr & 0xf0ff'ffff) | ((core->regs.fpscr & 0xf000'0000) >> 4);    \
}

#define FPRU(n) (core->regs.fpr[n].uval)
#define FPRD(n) (core->regs.fpr[n].dbl)
#define PS0(n)  (core->regs.fpr[n].dbl)
#define PS1(n)  (core->regs.ps1[n].dbl)
#define PS0U(n)  (core->regs.fpr[n].uval)
#define PS1U(n)  (core->regs.ps1[n].uval)


// Interpreter core API

#pragma once

namespace Gekko
{
	class Interpreter
	{
		friend GekkoCoreUnitTest::GekkoCoreUnitTest;

		GekkoCore* core = nullptr;

		void b();
		void ba();
		void bl();
		void bla();
		void bc();
		void bca();
		void bcl();
		void bcla();
		void bcctr();
		void bcctrl();
		void bclr();
		void bclrl();

		void cmpi();
		void cmp();
		void cmpli();
		void cmpl();

		void crand();
		void crandc();
		void creqv();
		void crnand();
		void crnor();
		void cror();
		void crorc();
		void crxor();
		void mcrf();

		void fadd();
		void fadd_d();
		void fadds();
		void fadds_d();
		void fdiv();
		void fdiv_d();
		void fdivs();
		void fdivs_d();
		void fmul();
		void fmul_d();
		void fmuls();
		void fmuls_d();
		void fres();
		void fres_d();
		void frsqrte();
		void frsqrte_d();
		void fsub();
		void fsub_d();
		void fsubs();
		void fsubs_d();
		void fsel();
		void fsel_d();
		void fmadd();
		void fmadd_d();
		void fmadds();
		void fmadds_d();
		void fmsub();
		void fmsub_d();
		void fmsubs();
		void fmsubs_d();
		void fnmadd();
		void fnmadd_d();
		void fnmadds();
		void fnmadds_d();
		void fnmsub();
		void fnmsub_d();
		void fnmsubs();
		void fnmsubs_d();
		void fctiw();
		void fctiw_d();
		void fctiwz();
		void fctiwz_d();
		void frsp();
		void frsp_d();
		void fcmpo();
		void fcmpu();
		void fabs();
		void fabs_d();
		void fmr();
		void fmr_d();
		void fnabs();
		void fnabs_d();
		void fneg();
		void fneg_d();

		void mcrfs();
		void mffs();
		void mffs_d();
		void mtfsb0();
		void mtfsb0_d();
		void mtfsb1();
		void mtfsb1_d();
		void mtfsf();
		void mtfsf_d();
		void mtfsfi();
		void mtfsfi_d();

		void lfd();
		void lfdu();
		void lfdux();
		void lfdx();
		void lfs();
		void lfsu();
		void lfsux();
		void lfsx();
		void stfd();
		void stfdu();
		void stfdux();
		void stfdx();
		void stfiwx();
		void stfs();
		void stfsu();
		void stfsux();
		void stfsx();

		void add();
		void add_d();
		void addo();
		void addo_d();
		void addc();
		void addc_d();
		void addco();
		void addco_d();
		void adde();
		void adde_d();
		void addeo();
		void addeo_d();
		void addi();
		void addic();
		void addic_d();
		void addis();
		void addme();
		void addme_d();
		void addmeo();
		void addmeo_d();
		void addze();
		void addze_d();
		void addzeo();
		void addzeo_d();
		void divw();
		void divw_d();
		void divwo();
		void divwo_d();
		void divwu();
		void divwu_d();
		void divwuo();
		void divwuo_d();
		void mulhw();
		void mulhw_d();
		void mulhwu();
		void mulhwu_d();
		void mulli();
		void mullw();
		void mullw_d();
		void mullwo();
		void mullwo_d();
		void neg();
		void neg_d();
		void nego();
		void nego_d();
		void subf();
		void subf_d();
		void subfo();
		void subfo_d();
		void subfc();
		void subfc_d();
		void subfco();
		void subfco_d();
		void subfe();
		void subfe_d();
		void subfeo();
		void subfeo_d();
		void subfic();
		void subfme();
		void subfme_d();
		void subfmeo();
		void subfmeo_d();
		void subfze();
		void subfze_d();
		void subfzeo();
		void subfzeo_d();

		void lbz();
		void lbzu();
		void lbzux();
		void lbzx();
		void lha();
		void lhau();
		void lhaux();
		void lhax();
		void lhz();
		void lhzu();
		void lhzux();
		void lhzx();
		void lwz();
		void lwzu();
		void lwzux();
		void lwzx();
		void stb();
		void stbu();
		void stbux();
		void stbx();
		void sth();
		void sthu();
		void sthux();
		void sthx();
		void stw();
		void stwu();
		void stwux();
		void stwx();
		void lhbrx();
		void lwbrx();
		void sthbrx();
		void stwbrx();
		void lmw();
		void stmw();
		void lswi();
		void lswx();
		void stswi();
		void stswx();

		void _and();
		void and_d();
		void andc();
		void andc_d();
		void andi_d();
		void andis_d();
		void cntlzw();
		void cntlzw_d();
		void eqv();
		void eqv_d();
		void extsb();
		void extsb_d();
		void extsh();
		void extsh_d();
		void nand();
		void nand_d();
		void nor();
		void nor_d();
		void _or();
		void or_d();
		void orc();
		void orc_d();
		void ori();
		void oris();
		void _xor();
		void xor_d();
		void xori();
		void xoris();

		void ps_div();
		void ps_div_d();
		void ps_sub();
		void ps_sub_d();
		void ps_add();
		void ps_add_d();
		void ps_sel();
		void ps_sel_d();
		void ps_res();
		void ps_res_d();
		void ps_mul();
		void ps_mul_d();
		void ps_rsqrte();
		void ps_rsqrte_d();
		void ps_msub();
		void ps_msub_d();
		void ps_madd();
		void ps_madd_d();
		void ps_nmsub();
		void ps_nmsub_d();
		void ps_nmadd();
		void ps_nmadd_d();
		void ps_neg();
		void ps_neg_d();
		void ps_mr();
		void ps_mr_d();
		void ps_nabs();
		void ps_nabs_d();
		void ps_abs();
		void ps_abs_d();

		void ps_sum0();
		void ps_sum0_d();
		void ps_sum1();
		void ps_sum1_d();
		void ps_muls0();
		void ps_muls0_d();
		void ps_muls1();
		void ps_muls1_d();
		void ps_madds0();
		void ps_madds0_d();
		void ps_madds1();
		void ps_madds1_d();
		void ps_cmpu0();
		void ps_cmpo0();
		void ps_cmpu1();
		void ps_cmpo1();
		void ps_merge00();
		void ps_merge00_d();
		void ps_merge01();
		void ps_merge01_d();
		void ps_merge10();
		void ps_merge10_d();
		void ps_merge11();
		void ps_merge11_d();

		void psq_lx();
		void psq_stx();
		void psq_lux();
		void psq_stux();
		void psq_l();
		void psq_lu();
		void psq_st();
		void psq_stu();

		void rlwimi();
		void rlwimi_d();
		void rlwinm();
		void rlwinm_d();
		void rlwnm();
		void rlwnm_d();

		void slw();
		void slw_d();
		void sraw();
		void sraw_d();
		void srawi();
		void srawi_d();
		void srw();
		void srw_d();

		void eieio();
		void isync();
		void lwarx();
		void stwcx_d();
		void sync();
		void rfi();
		void sc();
		void tw();
		void twi();
		void mcrxr();
		void mfcr();
		void mfmsr();
		void mfspr();
		void mftb();
		void mtcrf();
		void mtmsr();
		void mtspr();
		void dcbf();
		void dcbi();
		void dcbst();
		void dcbt();
		void dcbtst();
		void dcbz();
		void dcbz_l();
		void icbi();
		void mfsr();
		void mfsrin();
		void mtsr();
		void mtsrin();
		void tlbie();
		void tlbsync();
		void eciwx();
		void ecowx();

		void callvm();

		uint32_t    rotmask[32][32];    // mask for integer rotate opcodes 
		float       ldScale[64];        // for paired-single loads
		float       stScale[64];        // for paired-single stores

		float dequantize(uint32_t data, GEKKO_QUANT_TYPE type, uint8_t scale);
		uint32_t quantize(float data, GEKKO_QUANT_TYPE type, uint8_t scale);

		void BranchCheck();
		bool BcTest();
		bool BctrTest();

		template <typename T>
		inline void CmpCommon(size_t crfd, T a, T b);

		void Dispatch();

		uint32_t CarryBit = 0;
		uint32_t OverflowBit = 0;

		uint32_t FullAdder(uint32_t a, uint32_t b);
		uint32_t Rotl32(size_t sa, uint32_t data);

		void SET_XER_SO() { core->regs.spr[Gekko::SPR::XER] |= GEKKO_XER_SO; }
		void SET_XER_OV() { core->regs.spr[Gekko::SPR::XER] |= GEKKO_XER_OV; }
		void SET_XER_CA() { core->regs.spr[Gekko::SPR::XER] |= GEKKO_XER_CA; }

		void RESET_XER_SO() { core->regs.spr[Gekko::SPR::XER] &= ~GEKKO_XER_SO; }
		void RESET_XER_OV() { core->regs.spr[Gekko::SPR::XER] &= ~GEKKO_XER_OV; }
		void RESET_XER_CA() { core->regs.spr[Gekko::SPR::XER] &= ~GEKKO_XER_CA; }

		/// <summary>
		/// Current decoded instruction.
		/// </summary>
		DecoderInfo info = { 0 };

	public:
		Interpreter(GekkoCore* _core);
		~Interpreter() {}

		void ExecuteOpcode();

		uint32_t GetRotMask(int mb, int me);
	};

}
