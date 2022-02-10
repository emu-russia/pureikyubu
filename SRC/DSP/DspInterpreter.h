// GameCube DSP interpreter

#pragma once

namespace DSP
{
	class DspInterpreter
	{
		DspCore* core;

		// Regular instructions (single-word)

		void jmp();
		void call();
		void rets();
		void reti();
		void trap();
		void wait();
		void exec();
		void loop();
		void rep();
		void pld();
		void mr();
		void adsi();
		void adli();
		void cmpsi();
		void cmpli();
		void lsfi();
		void asfi();
		void xorli();
		void anli();
		void orli();
		void norm();
		void div();
		void addc();
		void subc();
		void negc();
		void _max();
		void lsf();
		void asf();
		void ld();
		void st();
		void ldsa();
		void stsa();
		void ldla();
		void stla();
		void mv();
		void mvsi();
		void mvli();
		void stli();
		void clr();
		void set();
		void btstl();
		void btsth();

		// Parallel instructions that occupy the upper part (in the lower part there is a parallel Load / Store / Move instruction)

		void p_add();
		void p_addl();
		void p_sub();
		void p_amv();
		void p_cmp();
		void p_inc();
		void p_dec();
		void p_abs();
		void p_neg();
		void p_clr();
		void p_rnd();
		void p_rndp();
		void p_tst();
		void p_lsl16();
		void p_lsr16();
		void p_asr16();
		void p_addp();
		void p_set();
		void p_mpy();
		void p_mac();
		void p_macn();
		void p_mvmpy();
		void p_rnmpy();
		void p_admpy();
		void p_not();
		void p_xor();
		void p_and();
		void p_or();
		void p_lsf();
		void p_asf();

		// Parallel mem opcodes (low part)

		void p_ldd();
		void p_ls();
		void p_ld();
		void p_st();
		void p_mv();
		void p_mr();

		// Helpers

		void FetchMpyParams(DspParameter s1p, DspParameter s2p, int64_t& s1, int64_t& s2, bool checkDp);
		void AdvanceAddress(int r, DspParameter param);
		bool ConditionTrue(ConditionCode cc);
		void Dispatch();

		/// <summary>
		/// Current decoded instruction.
		/// </summary>
		DecoderInfo info = { 0 };

		bool flowControl = false;

	public:
		DspInterpreter(DspCore * parent);
		~DspInterpreter();

		void ExecuteInstr();

	};
}
