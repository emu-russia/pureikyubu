// GameCube DSP interpreter

#pragma once

namespace DSP
{
	class DspInterpreter
	{
		DspCore* core;

		// Instructions

		void ABS(AnalyzeInfo& info);

		void ADD(AnalyzeInfo& info);
		void ADDARN(AnalyzeInfo& info);
		void ADDAX(AnalyzeInfo& info);
		void ADDAXL(AnalyzeInfo& info);
		void ADDI(AnalyzeInfo& info);
		void ADDIS(AnalyzeInfo& info);
		void ADDP(AnalyzeInfo& info);
		void ADDPAXZ(AnalyzeInfo& info);
		void ADDR(AnalyzeInfo& info);

		void ANDC(AnalyzeInfo& info);
		void ANDCF(AnalyzeInfo& info);
		void ANDF(AnalyzeInfo& info);
		void ANDI(AnalyzeInfo& info);
		void ANDR(AnalyzeInfo& info);

		void ASL(AnalyzeInfo& info);
		void ASR(AnalyzeInfo& info);
		void ASR16(AnalyzeInfo& info);

		void BLOOP(AnalyzeInfo& info);
		void BLOOPI(AnalyzeInfo& info);
		void CALLcc(AnalyzeInfo& info);
		void CALLR(AnalyzeInfo& info);

		void CLR(AnalyzeInfo& info);
		void CLRL(AnalyzeInfo& info);
		void CLRP(AnalyzeInfo& info);

		void CMP(AnalyzeInfo& info);
		void CMPI(AnalyzeInfo& info);
		void CMPIS(AnalyzeInfo& info);
		void CMPAXH(AnalyzeInfo& info);

		void DAR(AnalyzeInfo& info);
		void DEC(AnalyzeInfo& info);
		void DECM(AnalyzeInfo& info);

		void HALT(AnalyzeInfo& info);

		void IAR(AnalyzeInfo& info);

		void IFcc(AnalyzeInfo& info);

		void ILRR(AnalyzeInfo& info);
		void ILRRD(AnalyzeInfo& info);
		void ILRRI(AnalyzeInfo& info);
		void ILRRN(AnalyzeInfo& info);

		void INC(AnalyzeInfo& info);
		void INCM(AnalyzeInfo& info);

		void Jcc(AnalyzeInfo& info);
		void JMPR(AnalyzeInfo& info);
		void LOOP(AnalyzeInfo& info);
		void LOOPI(AnalyzeInfo& info);

		void LR(AnalyzeInfo& info);
		void LRI(AnalyzeInfo& info);
		void LRIS(AnalyzeInfo& info);
		void LRR(AnalyzeInfo& info);
		void LRRD(AnalyzeInfo& info);
		void LRRI(AnalyzeInfo& info);
		void LRRN(AnalyzeInfo& info);
		void LRS(AnalyzeInfo& info);

		void LSL(AnalyzeInfo& info);
		void LSL16(AnalyzeInfo& info);
		void LSR(AnalyzeInfo& info);
		void LSR16(AnalyzeInfo& info);

		void M2(AnalyzeInfo& info);
		void M0(AnalyzeInfo& info);
		void CLR15(AnalyzeInfo& info);
		void SET15(AnalyzeInfo& info);
		void CLR40(AnalyzeInfo& info);
		void SET40(AnalyzeInfo& info);

		void MOV(AnalyzeInfo& info);
		void MOVAX(AnalyzeInfo& info);
		void MOVNP(AnalyzeInfo& info);
		void MOVP(AnalyzeInfo& info);
		void MOVPZ(AnalyzeInfo& info);
		void MOVR(AnalyzeInfo& info);
		void MRR(AnalyzeInfo& info);

		void MADD(AnalyzeInfo& info);
		void MADDC(AnalyzeInfo& info);
		void MADDX(AnalyzeInfo& info);
		void MSUB(AnalyzeInfo& info);
		void MSUBC(AnalyzeInfo& info);
		void MSUBX(AnalyzeInfo& info);
		void MUL(AnalyzeInfo& info);
		void MULAC(AnalyzeInfo& info);
		void MULC(AnalyzeInfo& info);
		void MULCAC(AnalyzeInfo& info);
		void MULCMV(AnalyzeInfo& info);
		void MULCMVZ(AnalyzeInfo& info);
		void MULMV(AnalyzeInfo& info);
		void MULMVZ(AnalyzeInfo& info);
		void MULX(AnalyzeInfo& info);
		void MULXAC(AnalyzeInfo& info);
		void MULXMV(AnalyzeInfo& info);
		void MULXMVZ(AnalyzeInfo& info);

		void NEG(AnalyzeInfo& info);

		void ORC(AnalyzeInfo& info);
		void ORI(AnalyzeInfo& info);
		void ORR(AnalyzeInfo& info);

		void RETcc(AnalyzeInfo& info);
		void RTI(AnalyzeInfo& info);

		void SBSET(AnalyzeInfo& info);
		void SBCLR(AnalyzeInfo& info);

		void SI(AnalyzeInfo& info);
		void SR(AnalyzeInfo& info);
		void SRR(AnalyzeInfo& info);
		void SRRD(AnalyzeInfo& info);
		void SRRI(AnalyzeInfo& info);
		void SRRN(AnalyzeInfo& info);
		void SRS(AnalyzeInfo& info);

		void SUB(AnalyzeInfo& info);
		void SUBAX(AnalyzeInfo& info);
		void SUBP(AnalyzeInfo& info);
		void SUBR(AnalyzeInfo& info);

		void TST(AnalyzeInfo& info);
		void TSTAXH(AnalyzeInfo& info);

		void XORI(AnalyzeInfo& info);
		void XORR(AnalyzeInfo& info);

		// Packed instructions

		void DR(AnalyzeInfo& info);
		void IR(AnalyzeInfo& info);
		void NR(AnalyzeInfo& info);
		void MV(AnalyzeInfo& info);
		void S(AnalyzeInfo& info);
		void SN(AnalyzeInfo& info);
		void L(AnalyzeInfo& info);
		void LN(AnalyzeInfo& info);

		void LS(AnalyzeInfo& info);
		void SL(AnalyzeInfo& info);
		void LSN(AnalyzeInfo& info);
		void SLN(AnalyzeInfo& info);
		void LSM(AnalyzeInfo& info);
		void SLM(AnalyzeInfo& info);
		void LSNM(AnalyzeInfo& info);
		void SLNM(AnalyzeInfo& info);

		void LD(AnalyzeInfo& info);
		void LDN(AnalyzeInfo& info);
		void LDM(AnalyzeInfo& info);
		void LDNM(AnalyzeInfo& info);

		void LDAX(AnalyzeInfo& info);
		void LDAXN(AnalyzeInfo& info);
		void LDAXM(AnalyzeInfo& info);
		void LDAXNM(AnalyzeInfo& info);

		static int64_t SignExtend40(int64_t);
		bool Condition(ConditionCode cc);
		void Flags40(int64_t ac);
		void Flags(DspLongAccumulator ac);
		void Dispatch(AnalyzeInfo& info);

		// TODO: Cache analyzeinfo for IROM addresses

	public:
		DspInterpreter(DspCore * parent);
		~DspInterpreter();

		void ExecuteInstr();

	};
}
