
#pragma once

namespace Gekko
{

	enum class Instruction : int
	{
		Unknown = -1,

		// Integer Arithmetic Instructions
		addi,
		addis,
		add,
		add_d,
		addo,
		addo_d,
		subf,
		subf_d,
		subfo,
		subfo_d,
		addic,
		addic_d,
		subfic,
		addc,
		addc_d,
		addco,
		addco_d,
		subfc,
		subfc_d,
		subfco,
		subfco_d,
		adde,
		adde_d,
		addeo,
		addeo_d,
		subfe,
		subfe_d,
		subfeo,
		subfeo_d,
		addme,
		addme_d,
		addmeo,
		addmeo_d,
		subfme,
		subfme_d,
		subfmeo,
		subfmeo_d,
		addze,
		addze_d,
		addzeo,
		addzeo_d,
		subfze,
		subfze_d,
		subfzeo,
		subfzeo_d,
		neg,
		neg_d,
		nego,
		nego_d,
		mulli,
		mullw,
		mullw_d,
		mullwo,
		mullwo_d,
		mulhw,
		mulhw_d,
		mulhwu,
		mulhwu_d,
		divw,
		divw_d,
		divwo,
		divwo_d,
		divwu,
		divwu_d,
		divwuo,
		divwuo_d,

		// Integer Compare Instructions
		cmpi,
		cmp,
		cmpli,
		cmpl,

		// Integer Logical Instructions 
		andi_d,
		andis_d,
		ori,
		oris,
		xori,
		xoris,
		and,
		and_d,
		or,
		or_d,
		xor,
		xor_d,
		nand,
		nand_d,
		nor,
		nor_d,
		eqv,
		eqv_d,
		andc,
		andc_d,
		orc,
		orc_d,
		extsb,
		extsb_d,
		extsh,
		extsh_d,
		cntlzw,
		cntlzw_d,

		// Integer Rotate Instructions
		rlwinm,
		rlwinm_d,
		rlwnm,
		rlwnm_d,
		rlwimi,
		rlwimi_d,

		// Integer Shift Instructions 
		slw,
		slw_d,
		srw,
		srw_d,
		srawi,
		srawi_d,
		sraw,
		sraw_d,

		// Floating-Point Instructions 
		fadd,
		fadd_d,
		fadds,
		fadds_d,
		fsub,
		fsub_d,
		fsubs,
		fsubs_d,
		fmul,
		fmul_d,
		fmuls,
		fmuls_d,
		fdiv,
		fdiv_d,
		fdivs,
		fdivs_d,
		fres,
		fres_d,
		frsqrte,
		frsqrte_d,
		fsel,
		fsel_d,

		fmadd,
		fmadd_d,
		fmadds,
		fmadds_d,
		fmsub,
		fmsub_d,
		fmsubs,
		fmsubs_d,
		fnmadd,
		fnmadd_d,
		fnmadds,
		fnmadds_d,
		fnmsub,
		fnmsub_d,
		fnmsubs,
		fnmsubs_d,

		frsp,
		frsp_d,
		fctiw,
		fctiw_d,
		fctiwz,
		fctiwz_d,

		fcmpu,
		fcmpo,

		mffs,
		mffs_d,
		mcrfs,
		mtfsfi,
		mtfsfi_d,
		mtfsf,
		mtfsf_d,
		mtfsb0,
		mtfsb0_d,
		mtfsb1,
		mtfsb1_d,

		fmr,
		fmr_d,
		fneg,
		fneg_d,
		fabs,
		fabs_d,
		fnabs,
		fnabs_d,

		// Paired Single Instructions 
		ps_add,
		ps_add_d,
		ps_sub,
		ps_sub_d,
		ps_mul,
		ps_mul_d,
		ps_div,
		ps_div_d,
		ps_res,
		ps_res_d,
		ps_rsqrte,
		ps_rsqrte_d,
		ps_sel,
		ps_sel_d,
		ps_muls0,
		ps_muls0_d,
		ps_muls1,
		ps_muls1_d,
		ps_sum0,
		ps_sum0_d,
		ps_sum1,
		ps_sum1_d,

		ps_madd,
		ps_madd_d,
		ps_msub,
		ps_msub_d,
		ps_nmadd,
		ps_nmadd_d,
		ps_nmsub,
		ps_nmsub_d,
		ps_madds0,
		ps_madds0_d,
		ps_madds1,
		ps_madds1_d,

		ps_cmpu0,
		ps_cmpu1,
		ps_cmpo0,
		ps_cmpo1,

		ps_mr,
		ps_mr_d,
		ps_neg,
		ps_neg_d,
		ps_abs,
		ps_abs_d,
		ps_nabs,
		ps_nabs_d,
		ps_merge00,
		ps_merge00_d,
		ps_merge01,
		ps_merge01_d,
		ps_merge10,
		ps_merge10_d,
		ps_merge11,
		ps_merge11_d,

		// Integer Load Instructions 
		lbz,
		lbzx,
		lbzu,
		lbzux,
		lhz,
		lhzx,
		lhzu,
		lhzux,
		lha,
		lhax,
		lhau,
		lhaux,
		lwz,
		lwzx,
		lwzu,
		lwzux,

		stb,
		stbx,
		stbu,
		stbux,
		sth,
		sthx,
		sthu,
		sthux,
		stw,
		stwx,
		stwu,
		stwux,

		lhbrx,
		lwbrx,
		sthbrx,
		stwbrx,

		lmw,
		stmw,

		lswi,
		lswx,
		stswi,
		stswx,

		// Floating-Point Load Instructions 
		lfs,
		lfsx,
		lfsu,
		lfsux,
		lfd,
		lfdx,
		lfdu,
		lfdux,

		stfs,
		stfsx,
		stfsu,
		stfsux,
		stfd,
		stfdx,
		stfdu,
		stfdux,
		stfiwx,

		// Paired Single Load and Store Instructions
		psq_l,
		psq_lx,
		psq_lu,
		psq_lux,
		psq_st,
		psq_stx,
		psq_stu,
		psq_stux,

		// Branch Instructions
		b,
		ba,
		bl,
		bla,
		bc,
		bca,
		bcl,
		bcla,
		bclr,
		bclrl,
		bcctr,
		bcctrl,

		// Condition Register Instructions
		crand,
		cror,
		crxor,
		crnand,
		crnor,
		creqv,
		crandc,
		crorc,
		mcrf,
		mtcrf,
		mcrxr,
		mfcr,

		// System-related
		twi,
		tw,
		sc,
		mtspr,
		mfspr,
		lwarx,
		stwcx_d,
		sync,
		mftb,
		eieio,
		isync,
		dcbt,
		dcbtst,
		dcbz,
		dcbz_l,
		dcbst,
		dcbf,
		icbi,
		dcbi,
		eciwx,
		ecowx,
		rfi,
		mtmsr,
		mfmsr,
		mtsr,
		mtsrin,
		mfsr,
		mfsrin,
		tlbie,
		tlbsync,

	};

	enum class Param : int
	{
		Unknown = -1,
		Reg,
		FReg,
		Simm,
		Uimm,
		Crf,
		RegOffset,
		Num,
		Spr,
		Sr,
		Tbr,
		Crb,
		CRM,
		FM,
		Address,
	};

	typedef struct _AnalyzeInfo
	{
		uint32_t	instrBits;
		Instruction instr;

		size_t numParam;
		Param param[5];
		int paramBits[5];

		union
		{
			int16_t Signed;
			uint16_t Unsigned;
			uint32_t Address;
		} Imm;

		uint32_t pc;

		bool flow;

	} AnalyzeInfo;

	class Analyzer
	{
		static void OpMain(uint32_t instr, AnalyzeInfo* info);
		static void Op19(uint32_t instr, AnalyzeInfo* info);
		static void Op31(uint32_t instr, AnalyzeInfo* info);
		static void Op59(uint32_t instr, AnalyzeInfo* info);
		static void Op63(uint32_t instr, AnalyzeInfo* info);
		static void Op4(uint32_t instr, AnalyzeInfo* info);

		static void OpMainFast(uint32_t instr, AnalyzeInfo* info);
		static void Op19Fast(uint32_t instr, AnalyzeInfo* info);
		static void Op31Fast(uint32_t instr, AnalyzeInfo* info);
		static void Op59Fast(uint32_t instr, AnalyzeInfo* info);
		static void Op63Fast(uint32_t instr, AnalyzeInfo* info);
		static void Op4Fast(uint32_t instr, AnalyzeInfo* info);

		static void Dab(uint32_t instr, AnalyzeInfo* info);
		static void DabFast(uint32_t instr, AnalyzeInfo* info);
		static void DaSimm(uint32_t instr, AnalyzeInfo* info);
		static void DaSimmFast(uint32_t instr, AnalyzeInfo* info);
		static void Da(uint32_t instr, AnalyzeInfo* info);
		static void DaFast(uint32_t instr, AnalyzeInfo* info);
		static void Asb(uint32_t instr, AnalyzeInfo* info);
		static void AsbFast(uint32_t instr, AnalyzeInfo* info);
		static void AsUimm(uint32_t instr, AnalyzeInfo* info);
		static void AsUimmFast(uint32_t instr, AnalyzeInfo* info);
		static void TargetAddr(uint32_t instr, AnalyzeInfo* info);
		static void TargetAddrFast(uint32_t instr, AnalyzeInfo* info);
		static void BoBiTargetAddr(uint32_t instr, AnalyzeInfo* info);
		static void BoBiTargetAddrFast(uint32_t instr, AnalyzeInfo* info);
		static void BoBi(uint32_t instr, AnalyzeInfo* info);
		static void BoBiFast(uint32_t instr, AnalyzeInfo* info);
		static void CrfDab(uint32_t instr, AnalyzeInfo* info);
		static void CrfDabFast(uint32_t instr, AnalyzeInfo* info);
		static void CrfDaSimm(uint32_t instr, AnalyzeInfo* info);
		static void CrfDaSimmFast(uint32_t instr, AnalyzeInfo* info);
		static void CrfDaUimm(uint32_t instr, AnalyzeInfo* info);
		static void CrfDaUimmFast(uint32_t instr, AnalyzeInfo* info);
		static void As(uint32_t instr, AnalyzeInfo* info);
		static void AsFast(uint32_t instr, AnalyzeInfo* info);
		static void CrbDab(uint32_t instr, AnalyzeInfo* info);
		static void CrbDabFast(uint32_t instr, AnalyzeInfo* info);
		static void Ab(uint32_t instr, AnalyzeInfo* info);
		static void AbFast(uint32_t instr, AnalyzeInfo* info);
		static void FrDb(uint32_t instr, AnalyzeInfo* info);
		static void FrDbFast(uint32_t instr, AnalyzeInfo* info);
		static void FrDab(uint32_t instr, AnalyzeInfo* info);
		static void FrDabFast(uint32_t instr, AnalyzeInfo* info);
		static void CrfdFrAb(uint32_t instr, AnalyzeInfo* info);
		static void CrfdFrAbFast(uint32_t instr, AnalyzeInfo* info);
		static void FrDacb(uint32_t instr, AnalyzeInfo* info);
		static void FrDacbFast(uint32_t instr, AnalyzeInfo* info);
		static void FrDac(uint32_t instr, AnalyzeInfo* info);
		static void FrDacFast(uint32_t instr, AnalyzeInfo* info);

	public:

		static void Analyze(uint32_t pc, uint32_t instr, AnalyzeInfo* info);

		// The fast version is used if the user knows the number of parameters.
		static void AnalyzeFast(uint32_t pc, uint32_t instr, AnalyzeInfo* info);
	};

}
