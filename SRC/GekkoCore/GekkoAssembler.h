// Gekko Assembler.
// Used to support UVNA complementarity.

// https://github.com/ogamespec/dolwin-python/blob/master/Scripts/GekkoAssemblerUnitTests.py

#pragma once

namespace Gekko
{
	class GekkoAssembler
	{

	public:

		/// <summary>
		/// Build a Gekko instruction based on the current pc value and information from the `AnalyzeInfo` structure.
		/// </summary>
		/// <param name="info">Instruction information</param>
		/// <returns>info->instrBits: Gekko instruction (32 bit)</returns>
		static void Assemble(AnalyzeInfo* info);

	};
}
