// Gekko Assembler.
// Used to support UVNA complementarity.

// https://github.com/ogamespec/dolwin-python/blob/master/Scripts/GekkoAssemblerUnitTests.py

#pragma once

namespace Gekko
{
	class GekkoAssembler
	{
		/// <summary>
		/// Check parameter
		/// </summary>
		/// <param name="info">Information about Gekko instruction. If the parameter does not match the required type, throw an exception</param>
		/// <param name="paramNum">Parameter number</param>
		/// <param name="should">What type should the parameter be</param>
		static void CheckParam(AnalyzeInfo& info, size_t paramNum, Param should);

		/// <summary>
		/// Pack the bit value into an instruction. Bit numbering in PowerPC format (msb = 0). If the value does not fit into the range, throw an exception.
		/// </summary>
		/// <param name="res">Intermediate result</param>
		/// <param name="ppc_bit_from">Bit range start value (including)</param>
		/// <param name="ppc_bit_to">Bit range end value (including)</param>
		/// <param name="val">Integer value for packing</param>
		static void PackBits(uint32_t& res, size_t ppc_bit_from, size_t ppc_bit_to, size_t val);

		/// <summary>
		/// Set the specified bit. PowerPC bit numbering (msb = 0)
		/// </summary>
		/// <param name="res">Intermediate result</param>
		/// <param name="ppc_bit">Bit number to be set</param>
		static void SetBit(uint32_t& res, size_t ppc_bit);

		// It makes no sense to use the original names of the instruction forms, because there is a lot of confusion there.
		// Instead, the forms are named according to the composition of the parameters.

		/// <summary>
		/// rD, rA, rB version of `XO` instruction form.
		/// Example: addo. rD,rA,rB
		/// </summary>
		/// <param name="primary">Primary opcode value</param>
		/// <param name="extended">Extended opcode value</param>
		/// <param name="oe">OE bit</param>
		/// <param name="rc">Rc bit</param>
		/// <param name="info">Instruction information with parameters</param>
		/// <returns>info.instrBits: assembled Gekko instruction (32 bit)</returns>
		static void Form_DAB(size_t primary, size_t extended, bool oe, bool rc, AnalyzeInfo& info);

		/// <summary>
		/// Shorter XO version.
		/// Example: addme rD,rA
		/// </summary>
		static void Form_DA(size_t primary, size_t extended, bool oe, bool rc, AnalyzeInfo& info);

		/// <summary>
		/// Example: and. rA,rS,rB
		/// </summary>
		static void Form_ASB(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);

		/// <summary>
		/// SIMM version of `D` instruction form.
		/// Example: addic. rD,rA,SIMM
		/// </summary>
		/// <param name="primary">Primary opcode value</param>
		/// <param name="info">Instruction information with parameters</param>
		/// <returns>info.instrBits: assembled Gekko instruction (32 bit)</returns>
		static void Form_DASimm(size_t primary, AnalyzeInfo& info);

		/// <summary>
		/// Example: andi. rA,rS,UIMM
		/// </summary>
		static void Form_ASUimm(size_t primary, AnalyzeInfo& info);

		/// <summary>
		/// Long branch.
		/// Example: bl target_addr
		/// </summary>
		static void Form_BranchLong(size_t primary, bool aa, bool lk, AnalyzeInfo& info);

		/// <summary>
		/// Branch conditional by offset.
		/// Example: bc 12,0,target_addr
		/// </summary>
		static void Form_BranchShort(size_t primary, bool aa, bool lk, AnalyzeInfo& info);

		/// <summary>
		/// Branch CTR/LR BO, BI
		/// Example: bcctr 4, 10
		/// </summary>
		static void Form_BOBI(size_t primary, size_t extended, bool lk, AnalyzeInfo& info);

		// Further on the same principle

		static void Form_CrfDAB(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_CrfDCrfS(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_CrfDASimm(size_t primary, AnalyzeInfo& info);
		static void Form_CrfDAUimm(size_t primary, AnalyzeInfo& info);
		static void Form_AS(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);
		static void Form_CrbDAB(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_AB(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_SAB(size_t primary, size_t extended, bool oe, bool rc, AnalyzeInfo& info);
		static void Form_Extended(size_t primary, size_t extended, AnalyzeInfo& info);
		
		// Rotate/shift

		static void Form_AS_SHMBME(size_t primary, bool rc, AnalyzeInfo& info);
		static void Form_ASB_MBME(size_t primary, bool rc, AnalyzeInfo& info);
		static void Form_AS_SH(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);

		// Floating-Point

		static void Form_FrDAB(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);
		static void Form_FrDAC(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);
		static void Form_FrDB(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);
		static void Form_FrDACB(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);
		static void Form_FrD(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);
		static void Form_CrfDFrAB(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_CrbD(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);
		static void Form_Mtfsf(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);
		static void Form_Mtfsfi(size_t primary, size_t extended, bool rc, AnalyzeInfo& info);

		// Load and Store

		static void Form_DA_Offset(size_t primary, AnalyzeInfo& info);
		static void Form_SA_Offset(size_t primary, AnalyzeInfo& info);
		static void Form_DA_NB(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_SA_NB(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_FrDA_Offset(size_t primary, AnalyzeInfo& info);
		static void Form_FrSA_Offset(size_t primary, AnalyzeInfo& info);
		static void Form_FrDRegAB(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_FrSRegAB(size_t primary, size_t extended, AnalyzeInfo& info);

		// Trap Instructions

		static void Form_Trap(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_TrapImm(size_t primary, AnalyzeInfo& info);

		// Processor Control Instructions.

		static void Form_Mcrxr(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mfcr(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mfmsr(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mfspr(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mftb(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mtcrf(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mtmsr(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mtspr(size_t primary, size_t extended, AnalyzeInfo& info);

		// Segment Register Manipulation Instructions

		static void Form_Mfsr(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mfsrin(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mtsr(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_Mtsrin(size_t primary, size_t extended, AnalyzeInfo& info);

		// Lookaside Buffer Management Instructions

		static void Form_Tlbie(size_t primary, size_t extended, AnalyzeInfo& info);

		// Paired Single

		static void Form_PsqLoadStoreIndexed(size_t primary, size_t extended, AnalyzeInfo& info);
		static void Form_PsqLoadStore(size_t primary, AnalyzeInfo& info);


	public:

		/// <summary>
		/// Build a Gekko instruction based on the information from the `AnalyzeInfo` structure.
		/// All assembler errors are thrown as exceptions.
		/// </summary>
		/// <param name="info">Instruction information (including current pc value)</param>
		/// <returns>info.instrBits: Gekko instruction (32 bit)</returns>
		static void Assemble(AnalyzeInfo& info);

	};
}
