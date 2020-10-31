// Gekko Assembler.

#pragma once

namespace Gekko
{
	class GekkoAssembler
	{

	public:

		/// <summary>
		/// Build a Gekko instruction based on the current pc value and information from the `AnalyzeInfo` structure.
		/// </summary>
		/// <param name="pc">Current pc value</param>
		/// <param name="info">Instruction information</param>
		/// <returns>Gekko instruction (32 bit)</returns>
		static uint32_t Assemble(uint32_t pc, AnalyzeInfo* info);

	};
}
