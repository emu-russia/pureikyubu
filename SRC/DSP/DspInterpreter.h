// GameCube DSP interpreter

#pragma once

namespace DSP
{
	class DspInterpreter
	{
		DspCore* core;

		// Regular instructions (single-word)

		void jmp(DecoderInfo& info);
		void call(DecoderInfo& info);
		void rets(DecoderInfo& info);
		void reti(DecoderInfo& info);
		void trap(DecoderInfo& info);
		void wait(DecoderInfo& info);
		void exec(DecoderInfo& info);
		void loop(DecoderInfo& info);
		void rep(DecoderInfo& info);
		void pld(DecoderInfo& info);
		void mr(DecoderInfo& info);
		void adsi(DecoderInfo& info);
		void adli(DecoderInfo& info);
		void cmpsi(DecoderInfo& info);
		void cmpli(DecoderInfo& info);
		void lsfi(DecoderInfo& info);
		void asfi(DecoderInfo& info);
		void xorli(DecoderInfo& info);
		void anli(DecoderInfo& info);
		void orli(DecoderInfo& info);
		void norm(DecoderInfo& info);
		void div(DecoderInfo& info);
		void addc(DecoderInfo& info);
		void subc(DecoderInfo& info);
		void negc(DecoderInfo& info);
		void _max(DecoderInfo& info);
		void lsf(DecoderInfo& info);
		void asf(DecoderInfo& info);
		void ld(DecoderInfo& info);
		void st(DecoderInfo& info);
		void ldsa(DecoderInfo& info);
		void stsa(DecoderInfo& info);
		void ldla(DecoderInfo& info);
		void stla(DecoderInfo& info);
		void mv(DecoderInfo& info);
		void mvsi(DecoderInfo& info);
		void mvli(DecoderInfo& info);
		void stli(DecoderInfo& info);
		void clr(DecoderInfo& info);
		void set(DecoderInfo& info);
		void btstl(DecoderInfo& info);
		void btsth(DecoderInfo& info);

		// Parallel instructions that occupy the upper part (in the lower part there is a parallel Load / Store / Move instruction)

		void p_add(DecoderInfo& info);
		void p_addl(DecoderInfo& info);
		void p_sub(DecoderInfo& info);
		void p_amv(DecoderInfo& info);
		void p_cmp(DecoderInfo& info);
		void p_inc(DecoderInfo& info);
		void p_dec(DecoderInfo& info);
		void p_abs(DecoderInfo& info);
		void p_neg(DecoderInfo& info);
		void p_clr(DecoderInfo& info);
		void p_rnd(DecoderInfo& info);
		void p_rndp(DecoderInfo& info);
		void p_tst(DecoderInfo& info);
		void p_lsl16(DecoderInfo& info);
		void p_lsr16(DecoderInfo& info);
		void p_asr16(DecoderInfo& info);
		void p_addp(DecoderInfo& info);
		void p_set(DecoderInfo& info);
		void p_mpy(DecoderInfo& info);
		void p_mac(DecoderInfo& info);
		void p_macn(DecoderInfo& info);
		void p_mvmpy(DecoderInfo& info);
		void p_rnmpy(DecoderInfo& info);
		void p_admpy(DecoderInfo& info);
		void p_not(DecoderInfo& info);
		void p_xor(DecoderInfo& info);
		void p_and(DecoderInfo& info);
		void p_or(DecoderInfo& info);
		void p_lsf(DecoderInfo& info);
		void p_asf(DecoderInfo& info);

		// Parallel mem opcodes (low part)

		void p_ldd(DecoderInfo& info);
		void p_ls(DecoderInfo& info);
		void p_ld(DecoderInfo& info);
		void p_st(DecoderInfo& info);
		void p_mv(DecoderInfo& info);
		void p_mr(DecoderInfo& info);

		// Helpers

		void FetchMpyParams(DspParameter s1p, DspParameter s2p, int64_t& s1, int64_t& s2, bool checkDp);
		void AdvanceAddress(int r, DspParameter param);
		bool ConditionTrue(ConditionCode cc);
		void Dispatch(DecoderInfo& info);

		// TODO: Cache DecoderInfo?

		bool flowControl = false;

	public:
		DspInterpreter(DspCore * parent);
		~DspInterpreter();

		void ExecuteInstr();

	};
}
