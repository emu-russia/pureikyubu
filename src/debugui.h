/*

# DebugUI

The entire debug console UI has moved here.

This code is very oriented on the Win32 Console API, so it was natural to take it outside.

All debug instances work non-invasively, in their own thread, so they can be used at any time without disrupting the normal flow of system emulation.

In turn, the emulated system does not react in any way to the work (or not work) of the debugger, it simply does not know that it is being debugged.

As usual, let me remind you that the master driving force of the entire emulator is the GekkoCore thread.
When the thread is stopped (Gekko TBR does not increase its value), all other emulator systems (including DspCore) are also stopped.
This does not apply to the debugger (and any other UI), which are architecturally located above the emulator core and work in their own threads.

## Architectural features

All Win32 code for interacting with the Console API is integrated into Cui.cpp.
I'm not sure that someone would want to port the console debugger somewhere other than Windows (the Console API is very specific), but for convenience I brought all the code there.

All other modules are based on Cui, as custom CuiWindows.

## Interacting with emulator components

Communication with the Gekko and DSP cores is done through the Debug JDI Client.

You can watch registers, memory, manage breakpoints and all that stuff.

Gekko and DSP disassemblers are in the emulator core, in the corresponding components, JDI returns already disassembled text.

*/


#pragma once

#define DEBUG_UI_JDI_JSON "./Data/Json/DebugUIJdi.json"

namespace Debug
{
	void DebugUIReflector();
}

namespace Debug
{

	// Displaying message history. It can be used both by the system debugger and as part of the DSP Debugger.
	class ReportWindow : public CuiWindow
	{
		static const size_t maxMessages = 32 * 1024;

		Thread* thread = nullptr;

		static void ThreadProc(void* param);

		int messagePtr = 0;

		CuiColor ChannelNameToColor(const std::string& name);
		std::list<std::string> SplitMessages(std::string str);

	public:
		ReportWindow(CuiRect& rect, std::string name, Cui* parent);
		~ReportWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);
	};

}

namespace Debug
{

	enum class DebugMode
	{
		Ready = 0,
		Scrolling,
	};

	// Status window, to display the current state of the debugger
	class StatusWindow : public CuiWindow
	{
		DebugMode _mode = DebugMode::Ready;

	public:
		StatusWindow(CuiRect& rect, std::string name, Cui* parent);
		~StatusWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);

		void SetMode(DebugMode mode);
	};

}

namespace Debug
{

	class CmdlineWindow : public CuiWindow
	{
		const std::string prompt = "> ";

		size_t curpos = 0;			// Current cursor position
		std::string text;		// Current command line text

		std::vector<std::string> history;
		int historyPos = 0;

		bool TestEmpty();
		void SearchLeft();
		void SearchRight();
		void Process();
		void ClearCmdline();

	public:
		CmdlineWindow(CuiRect& rect, std::string name, Cui* parent);
		~CmdlineWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);
	};

}

// This module is used to communicate with the JDI host. The host can be on the network, in a pluggable DLL or statically linked.

namespace JDI
{
	typedef Json::Value* (*CmdDelegate)(std::vector<std::string>& args);
	typedef void (*JdiReflector)();
}

namespace Debug
{
	class JdiClient
	{
	public:

		JdiClient();
		~JdiClient();

		// Generic

		std::string GetVersion();
		void ExecuteCommand(const std::string& cmdline);

		// Generic debug

		std::string DebugChannelToString(int chan);
		void QueryDebugMessages(std::list<std::pair<int, std::string>>& queue);
		void Report(const std::string& text);
		bool IsLoaded();
		bool IsCommandExists(const std::string& cmdline);

		// Gekko

		bool IsRunning();
		void GekkoRun();
		void GekkoSuspend();
		void GekkoStep();
		void GekkoSkipInstruction();

		uint32_t GetGpr(size_t n);
		uint64_t GetPs0(size_t n);
		uint64_t GetPs1(size_t n);
		uint32_t GetPc();
		uint32_t GetMsr();
		uint32_t GetCr();
		uint32_t GetFpscr();
		uint32_t GetSpr(size_t n);
		uint32_t GetSr(size_t n);
		uint32_t GetTbu();
		uint32_t GetTbl();

		void* TranslateDMmu(uint32_t address);
		void* TranslateIMmu(uint32_t address);
		uint32_t VirtualToPhysicalDMmu(uint32_t address);
		uint32_t VirtualToPhysicalIMmu(uint32_t address);

		bool GekkoTestBreakpoint(uint32_t address);
		void GekkoToggleBreakpoint(uint32_t address);
		void GekkoAddOneShotBreakpoint(uint32_t address);

		std::string GekkoDisasm(uint32_t address);
		bool GekkoIsBranch(uint32_t address, uint32_t& targetAddress);

		uint32_t AddressByName(const std::string& name);
		std::string NameByAddress(uint32_t address);

		// DSP

		bool DspIsRunning();
		void DspRun();
		void DspSuspend();
		void DspStep();

		uint16_t DspGetReg(size_t n);
		uint16_t DspGetPsr();
		uint16_t DspGetPc();
		uint64_t DspPackProd();

		void* DspTranslateDMem(uint32_t address);
		void* DspTranslateIMem(uint32_t address);

		bool DspTestBreakpoint(uint32_t address);
		void DspToggleBreakpoint(uint32_t address);
		void DspAddOneShotBreakpoint(uint32_t address);

		std::string DspDisasm(uint32_t address, size_t& instrSizeWords, bool& flowControl);
		bool DspIsCall(uint32_t address, uint32_t& targetAddress);
		bool DspIsCallOrJump(uint32_t address, uint32_t& targetAddress);

	};

	extern JdiClient* Jdi;
}


namespace Debug
{

	enum class DisasmMode
	{
		GekkoDisasm,
		DSPDisasm,
	};

	class Disasm : public CuiWindow
	{
		DisasmMode mode = DisasmMode::GekkoDisasm;

		// Gekko Disasm

		uint32_t gekko_address = 0x8000'0000;
		uint32_t gekko_cursor = 0x8000'0000;

		int disa_sub_h = 0;

		std::vector<std::pair<uint32_t, uint32_t>> browseHist;

		bool IsCursorVisible();
		int DisasmLine(int line, uint32_t addr);
		
		// DSP IMEM Disasm

		// Do not forget that DSP addressing is done with freaking 16-bit slots (`words`)
		static const uint32_t IROM_START_ADDRESS = 0x8000;
		uint32_t dsp_current = 0x8000;
		uint32_t dsp_cursor = 0x8000;
		size_t wordsOnScreen = 0;
		std::vector<std::pair<uint32_t, uint32_t>> dspBrowseHist;

		void DspImemOnDraw();
		void DspImemOnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);

		void RotateView(bool forward);

	public:
		Disasm(CuiRect& rect, std::string name, Cui* parent);
		~Disasm();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);

		uint32_t GetCursor();
		void SetCursor(uint32_t address);

		DisasmMode GetMode() { return mode; }
		
		bool DspAddressVisible(uint32_t address);
		void DspEnsurePcVisible();
	};

}

namespace Debug
{
	enum class DebugRegmode
	{
		GPR = 0,
		FPR,
		PSR,
		MMU,
		DSP,
	};

#pragma pack(push, 1)

	union Fpreg
	{
		struct
		{
			uint32_t	Low;
			uint32_t	High;
		} u;
		uint64_t Raw;
		double Float;
	};

#pragma pack(pop)

	// DSP registers

	enum class DspReg
	{
		r0, r1, r2, r3,
		m0, m1, m2, m3,
		l0, l1, l2, l3,
		pcs, pss, eas, lcs,
		a2, b2, dpp, psr,
		ps0, ps1, ps2, pc1,
		x0, y0, x1, y1,
		a0, b0, a1, b1
	};

	class DebugRegs : public CuiWindow
	{
		DebugRegmode mode = DebugRegmode::GPR;

		// Used to highlight reg changes
		uint32_t savedGpr[32] = { 0 };
		Fpreg savedPs0[32] = { 0 };
		Fpreg savedPs1[32] = { 0 };

		void Memorize();

		void ShowGprs();
		void ShowOtherRegs();
		void ShowFprs();
		void ShowPairedSingle();
		void ShowMmu();

		void RotateView(bool forward);

		void print_gprreg(int x, int y, int num);
		void print_fpreg(int x, int y, int num);
		void print_ps(int x, int y, int num);
		int cntlzw(uint32_t val);
		void describe_bat_reg(int x, int y, uint32_t up, uint32_t lo, bool instr);
		std::string smart_size(size_t size);

		// DSP Regs

		uint16_t savedDspRegs[32] = { 0 };
		uint16_t savedDspPsr = 0;

		void ShowDspRegs();
		void DspDrawRegs();
		void DspDrawStatusBits();
		void DspPrintReg(int x, int y, DspReg n);
		void DspPrintPsrBit(int x, int y, int n);
		void DspMemorize();

	public:
		DebugRegs(CuiRect& rect, std::string name, Cui* parent);
		~DebugRegs();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);
	};

}


namespace Debug
{
	enum class MemoryViewMode
	{
		GekkoVirtual = 0,
		GekkoDataCache,
		MainMem,
		DSP_ARAM,		// Without an extension that hooks to an external port (HSP). We don't know how to emulate HSP yet
		DSP_DMEM,		// DRAM + DROM. The memory-mapped DSP registers are shown separately
	};

	class MemoryView : public CuiWindow
	{
		MemoryViewMode mode = MemoryViewMode::GekkoVirtual;
		uint32_t vm_cursor = 0x8000'0000;
		uint32_t dcache_cursor = 0;
		uint32_t mmem_cursor = 0;
		uint32_t aram_cursor = 0;

		void RotateView(bool forward);

		std::string hexbyte(uint32_t addr);
		char charbyte(uint32_t addr);

		// Do not forget that DSP addressing is done with freaking 16-bit slots (`words`)
		static const uint32_t DROM_START_ADDRESS = 0x1000;
		uint32_t dmem_current = 0;

		void DspDmemOnDraw();
		void DspDmemOnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);

	public:
		MemoryView(CuiRect& rect, std::string name, Cui* parent);
		~MemoryView();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);

		void SetCursor(uint32_t address);
	};

}



// System-wide debugger

namespace Debug
{

	class Debugger : public Cui
	{
		static const size_t width = 120;
		static const size_t height = 80;

		static const size_t regsHeight = 17;
		static const size_t memViewHeight = 8;
		static const size_t disaHeight = 28;

		DebugRegs* regs = nullptr;
		MemoryView* memview = nullptr;
		Disasm* disasm = nullptr;
		ReportWindow* msgs = nullptr;
		CmdlineWindow* cmdline = nullptr;
		StatusWindow* status = nullptr;

	public:
		Debugger();

		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);

		void SetMemoryCursor(uint32_t virtualAddress);
		void SetDisasmCursor(uint32_t virtualAddress);

	};
}

namespace Debug
{
	extern Debugger* debugger;
}
