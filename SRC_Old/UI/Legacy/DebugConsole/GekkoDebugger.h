// System-wide debugger

#pragma once

#include "GekkoRegs.h"
#include "MemoryView.h"
#include "GekkoDisasm.h"
#include "ReportView.h"
#include "Cmdline.h"
#include "StatusLine.h"

namespace Debug
{

	class GekkoDebug : public Cui
	{
		static const size_t width = 120;
		static const size_t height = 80;

		static const size_t regsHeight = 17;
		static const size_t memViewHeight = 8;
		static const size_t disaHeight = 28;

		GekkoRegs* regs = nullptr;
		MemoryView* memview = nullptr;
		GekkoDisasm* disasm = nullptr;
		ReportWindow* msgs = nullptr;
		CmdlineWindow* cmdline = nullptr;
		StatusWindow* status = nullptr;

	public:
		GekkoDebug();

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);

		void SetMemoryCursor(uint32_t virtualAddress);
		void SetDisasmCursor(uint32_t virtualAddress);

	};

}
