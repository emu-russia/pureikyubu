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

		/// <summary>
		/// rD, rA, rB version of `XO` instruction form. The value of the assembled instruction is placed in the `instrBits` property.
		/// Example: addo. rD,rA,rB
		/// </summary>
		/// <param name="primary">Primary opcode value</param>
		/// <param name="extended">Extended opcode value</param>
		/// <param name="oe">OE bit</param>
		/// <param name="rc">Rc bit</param>
		/// <param name="info">Instruction information with parameters</param>
		static void Form_XO(size_t primary, size_t extended, bool oe, bool rc, AnalyzeInfo& info);

		/// <summary>
		/// SIMM version of `D` instruction form. The value of the assembled instruction is placed in the `instrBits` property.
		/// Example: addic. rD,rA,SIMM
		/// </summary>
		/// <param name="primary">Primary opcode value</param>
		/// <param name="info">Instruction information with parameters</param>
		static void Form_D(size_t primary, AnalyzeInfo& info);

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
