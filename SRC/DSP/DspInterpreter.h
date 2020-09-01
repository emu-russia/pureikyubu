// GameCube DSP interpreter

#pragma once

namespace DSP
{
	class DspInterpreter
	{
		DspCore* core;

		// Regular instructions (single-word)

		void jmp(AnalyzeInfo& info);
		void call(AnalyzeInfo& info);
		void rets(AnalyzeInfo& info);
		void reti(AnalyzeInfo& info);
		void trap(AnalyzeInfo& info);
		void wait(AnalyzeInfo& info);
		void exec(AnalyzeInfo& info);
		void loop(AnalyzeInfo& info);
		void rep(AnalyzeInfo& info);
		void pld(AnalyzeInfo& info);
		void mr(AnalyzeInfo& info);
		void adsi(AnalyzeInfo& info);
		void adli(AnalyzeInfo& info);
		void cmpsi(AnalyzeInfo& info);
		void cmpli(AnalyzeInfo& info);
		void lsfi(AnalyzeInfo& info);
		void asfi(AnalyzeInfo& info);
		void xorli(AnalyzeInfo& info);
		void anli(AnalyzeInfo& info);
		void orli(AnalyzeInfo& info);
		void norm(AnalyzeInfo& info);
		void div(AnalyzeInfo& info);
		void addc(AnalyzeInfo& info);
		void subc(AnalyzeInfo& info);
		void negc(AnalyzeInfo& info);
		void _max(AnalyzeInfo& info);
		void lsf(AnalyzeInfo& info);
		void asf(AnalyzeInfo& info);
		void ld(AnalyzeInfo& info);
		void st(AnalyzeInfo& info);
		void ldsa(AnalyzeInfo& info);
		void stsa(AnalyzeInfo& info);
		void ldla(AnalyzeInfo& info);
		void stla(AnalyzeInfo& info);
		void mv(AnalyzeInfo& info);
		void mvsi(AnalyzeInfo& info);
		void mvli(AnalyzeInfo& info);
		void stli(AnalyzeInfo& info);
		void clr(AnalyzeInfo& info);
		void set(AnalyzeInfo& info);
		void btstl(AnalyzeInfo& info);
		void btsth(AnalyzeInfo& info);

		// Parallel instructions that occupy the upper part (in the lower part there is a parallel Load / Store / Move instruction)

		void p_add(AnalyzeInfo& info);
		void p_addl(AnalyzeInfo& info);
		void p_sub(AnalyzeInfo& info);
		void p_amv(AnalyzeInfo& info);
		void p_cmp(AnalyzeInfo& info);
		void p_inc(AnalyzeInfo& info);
		void p_dec(AnalyzeInfo& info);
		void p_abs(AnalyzeInfo& info);
		void p_neg(AnalyzeInfo& info);
		void p_clr(AnalyzeInfo& info);
		void p_rnd(AnalyzeInfo& info);
		void p_rndp(AnalyzeInfo& info);
		void p_tst(AnalyzeInfo& info);
		void p_lsl16(AnalyzeInfo& info);
		void p_lsr16(AnalyzeInfo& info);
		void p_asr16(AnalyzeInfo& info);
		void p_addp(AnalyzeInfo& info);
		void p_set(AnalyzeInfo& info);
		void p_mpy(AnalyzeInfo& info);
		void p_mac(AnalyzeInfo& info);
		void p_macn(AnalyzeInfo& info);
		void p_mvmpy(AnalyzeInfo& info);
		void p_rnmpy(AnalyzeInfo& info);
		void p_admpy(AnalyzeInfo& info);
		void p_not(AnalyzeInfo& info);
		void p_xor(AnalyzeInfo& info);
		void p_and(AnalyzeInfo& info);
		void p_or(AnalyzeInfo& info);
		void p_lsf(AnalyzeInfo& info);
		void p_asf(AnalyzeInfo& info);

		// Parallel mem opcodes (low part)

		void p_ldd(AnalyzeInfo& info);
		void p_ls(AnalyzeInfo& info);
		void p_ld(AnalyzeInfo& info);
		void p_st(AnalyzeInfo& info);
		void p_mv(AnalyzeInfo& info);
		void p_mr(AnalyzeInfo& info);

		// Helpers

		bool ConditionTrue(ConditionCode cc);
		void Dispatch(AnalyzeInfo& info);

		// TODO: Cache analyzeinfo?

	public:
		DspInterpreter(DspCore * parent);
		~DspInterpreter();

		void ExecuteInstr();

	};
}
