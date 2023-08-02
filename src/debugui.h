/*

# DebugConsole

The entire debug console UI has moved here.

This code is very oriented on the Win32 Console API, so it was natural to take it outside.

All debug instances work non-invasively, in their own thread, so they can be used at any time without disrupting the normal flow of system emulation.

In turn, the emulated system does not react in any way to the work (or not work) of the debugger, it simply does not know that it is being debugged.

As usual, let me remind you that the master driving force of the entire emulator is the GekkoCore thread.
When the thread is stopped (Gekko TBR does not increase its value), all other emulator systems (including DspCore) are also stopped.
This does not apply to the debugger (and any other UI), which are architecturally located above the emulator core and work in their own threads.

## Architectural features

Debug UI contains 2 instances:
- System-wide debugger focused on GekkoCore (code rewritten on Cui.cpp)
- DSP debugger (the code is based on the more convenient Cui.cpp from ground up)

The Win32 Console API does not allow you to create more than one console per process, so you can only use one instance at a time.

Debug UIs can be opened via the `Debug` menu.

## Cui

All Win32 code for interacting with the Console API is integrated into Cui.cpp.
I'm not sure that someone would want to port the console debugger somewhere other than Windows (the Console API is very specific), but for convenience I brought all the code there.

All other modules are based on Cui, as custom CuiWindows.

## Interacting with emulator components

Communication with the Gekko and DSP cores is done through the Debug JDI Client.

You can watch registers, memory, manage breakpoints and all that stuff.

Gekko and DSP disassemblers are in the emulator core, in the corresponding components, JDI returns already disassembled text.

*/


#pragma once

namespace Debug
{

	class ReportWindow : public CuiWindow
	{
		static const size_t maxMessages = 32 * 1024;

		Thread* thread = nullptr;

		static void ThreadProc(void* param);

		int messagePtr = 0;

		CuiColor ChannelNameToColor(const std::string& name);
		std::list<std::string> SplitMessages(std::string str);

	public:
		ReportWindow(RECT& rect, std::string name, Cui* parent);
		~ReportWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}

namespace Debug
{

	enum class DebugMode
	{
		Ready = 0,
		Scrolling,
	};

	class StatusWindow : public CuiWindow
	{
		DebugMode _mode = DebugMode::Ready;

	public:
		StatusWindow(RECT& rect, std::string name, Cui* parent);
		~StatusWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);

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
		CmdlineWindow(RECT& rect, std::string name, Cui* parent);
		~CmdlineWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}

// This module is used to communicate with the JDI host. The host can be on the network, in a pluggable DLL or statically linked.

namespace JDI
{
	typedef Json::Value* (*CmdDelegate)(std::vector<std::string>& args);
	typedef void (*JdiReflector)();
}

#ifdef _WINDOWS
typedef Json::Value* (__cdecl* CALL_JDI)(const char* request);
typedef bool(__cdecl* CALL_JDI_NO_RETURN)(const char* request);
typedef bool(__cdecl* CALL_JDI_RETURN_INT)(const char* request, int* valueOut);
typedef bool(__cdecl* CALL_JDI_RETURN_STRING)(const char* request, char* valueOut, size_t valueSize);
typedef bool(__cdecl* CALL_JDI_RETURN_BOOL)(const char* request, bool* valueOut);

typedef void(__cdecl* JDI_ADD_NODE)(const char* filename, JDI::JdiReflector reflector);
typedef void(__cdecl* JDI_REMOVE_NODE)(const char* filename);
typedef void(__cdecl* JDI_ADD_CMD)(const char* name, JDI::CmdDelegate command);
#endif

namespace Debug
{
	class JdiClient
	{
#ifdef _WINDOWS
		CALL_JDI CallJdi = nullptr;
		CALL_JDI_NO_RETURN CallJdiNoReturn = nullptr;
		CALL_JDI_RETURN_INT CallJdiReturnInt = nullptr;
		CALL_JDI_RETURN_STRING CallJdiReturnString = nullptr;
		CALL_JDI_RETURN_BOOL CallJdiReturnBool = nullptr;

		HMODULE dll = nullptr;
#endif

	public:

#ifdef _WINDOWS
		JDI_ADD_NODE JdiAddNode = nullptr;
		JDI_REMOVE_NODE JdiRemoveNode = nullptr;
		JDI_ADD_CMD JdiAddCmd = nullptr;
#endif

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

// Visual DSP Debugger

namespace Debug
{
	class DspDebug : public Cui
	{
		static const size_t width = 80;
		static const size_t height = 60;

		DspImem* imemWindow = nullptr;
		CmdlineWindow* cmdline = nullptr;

	public:
		DspDebug();

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}

// Do not forget that DSP addressing is done with freaking 16-bit slots (`words`)

namespace Debug
{

	class DspDmem : public CuiWindow
	{
		static const uint32_t DROM_START_ADDRESS = 0x1000;

		uint32_t current = 0;

	public:
		DspDmem(RECT& rect, std::string name, Cui* parent);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}

// Do not forget that DSP addressing is done with freaking 16-bit slots (`words`)

namespace Debug
{
	class DspDebug;

	class DspImem : public CuiWindow
	{
		friend DspDebug;

		static const uint32_t IROM_START_ADDRESS = 0x8000;

		uint32_t current = 0x8000;
		uint32_t cursor = 0x8000;
		size_t wordsOnScreen = 0;

		bool AddressVisible(uint32_t address);

		std::vector<std::pair<uint32_t, uint32_t>> browseHist;

	public:
		DspImem(RECT& rect, std::string name, Cui* parent);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}

namespace Debug
{
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

	class DspRegs : public CuiWindow
	{
		// Used to highlight reg changes
		uint16_t savedRegs[32] = { 0 };
		uint16_t savedPsr = 0;

		void DrawRegs();
		void DrawStatusBits();

		void PrintReg(int x, int y, DspReg n);
		void PrintPsrBit(int x, int y, int n);

		void Memorize();

	public:
		DspRegs(RECT& rect, std::string name, Cui* parent);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}

namespace Debug
{

	class GekkoDisasm : public CuiWindow
	{
		uint32_t address = 0x8000'0000;
		uint32_t cursor = 0x8000'0000;

		int disa_sub_h = 0;

		std::vector<std::pair<uint32_t, uint32_t>> browseHist;

		bool IsCursorVisible();
		int DisasmLine(int line, uint32_t addr);

	public:
		GekkoDisasm(RECT& rect, std::string name, Cui* parent);
		~GekkoDisasm();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);

		uint32_t GetCursor();
		void SetCursor(uint32_t address);
	};

}

namespace Debug
{

	// Machine State Flags
#define MSR_BIT(n)			(1 << (31-(n)))
#define MSR_RESERVED        0xFFFA0088
#define MSR_POW             (MSR_BIT(13))               // Power management enable
#define MSR_ILE             (MSR_BIT(15))               // Exception little-endian mode
#define MSR_EE              (MSR_BIT(16))               // External interrupt enable
#define MSR_PR              (MSR_BIT(17))               // User privilege level
#define MSR_FP              (MSR_BIT(18))               // Floating-point available
#define MSR_ME              (MSR_BIT(19))               // Machine check enable
#define MSR_FE0             (MSR_BIT(20))               // Floating-point exception mode 0
#define MSR_SE              (MSR_BIT(21))               // Single-step trace enable
#define MSR_BE              (MSR_BIT(22))               // Branch trace enable
#define MSR_FE1             (MSR_BIT(23))               // Floating-point exception mode 1
#define MSR_IP              (MSR_BIT(25))               // Exception prefix
#define MSR_IR              (MSR_BIT(26))               // Instruction address translation
#define MSR_DR              (MSR_BIT(27))               // Data address translation
#define MSR_PM              (MSR_BIT(29))               // Performance monitor mode
#define MSR_RI              (MSR_BIT(30))               // Recoverable exception
#define MSR_LE              (MSR_BIT(31))               // Little-endian mode enable

#define HID0_EMCP	0x8000'0000
#define HID0_DBP	0x4000'0000
#define HID0_EBA	0x2000'0000
#define HID0_EBD	0x1000'0000
#define HID0_BCLK	0x0800'0000
#define HID0_ECLK	0x0200'0000
#define HID0_PAR	0x0100'0000
#define HID0_DOZE	0x0080'0000
#define HID0_NAP	0x0040'0000
#define HID0_SLEEP	0x0020'0000
#define HID0_DPM	0x0010'0000
#define HID0_NHR	0x0001'0000			// Not hard reset (software-use only)
#define HID0_ICE	0x0000'8000
#define HID0_DCE	0x0000'4000
#define HID0_ILOCK	0x0000'2000
#define HID0_DLOCK	0x0000'1000
#define HID0_ICFI	0x0000'0800
#define HID0_DCFI	0x0000'0400
#define HID0_SPD	0x0000'0200
#define HID0_IFEM	0x0000'0100
#define HID0_SGE	0x0000'0080
#define HID0_DCFA	0x0000'0040
#define HID0_BTIC	0x0000'0020
#define HID0_ABE	0x0000'0008
#define HID0_BHT	0x0000'0004
#define HID0_NOOPTI	0x0000'0001

#define HID2_LSQE   0x8000'0000          // PS load/store quantization
#define HID2_WPE    0x4000'0000          // gathering enabled
#define HID2_PSE    0x2000'0000          // PS-mode
#define HID2_LCE    0x1000'0000          // locked cache enable

#define Gekko_SPR_XER 1
#define Gekko_SPR_LR 8
#define Gekko_SPR_CTR 9
#define Gekko_SPR_DSISR 18
#define Gekko_SPR_DAR 19
#define Gekko_SPR_DEC 22
#define Gekko_SPR_SDR1 25
#define Gekko_SPR_SRR0 26
#define Gekko_SPR_SRR1 27
#define Gekko_SPR_SPRG0 272
#define Gekko_SPR_SPRG1 273
#define Gekko_SPR_SPRG2 274
#define Gekko_SPR_SPRG3 275
#define Gekko_SPR_EAR 282
#define Gekko_SPR_TBL 284
#define Gekko_SPR_TBU 285
#define Gekko_SPR_PVR 287
#define Gekko_SPR_IBAT0U 528
#define Gekko_SPR_IBAT0L 529
#define Gekko_SPR_IBAT1U 530
#define Gekko_SPR_IBAT1L 531
#define Gekko_SPR_IBAT2U 532
#define Gekko_SPR_IBAT2L 533
#define Gekko_SPR_IBAT3U 534
#define Gekko_SPR_IBAT3L 535
#define Gekko_SPR_DBAT0U 536
#define Gekko_SPR_DBAT0L 537
#define Gekko_SPR_DBAT1U 538
#define Gekko_SPR_DBAT1L 539
#define Gekko_SPR_DBAT2U 540
#define Gekko_SPR_DBAT2L 541
#define Gekko_SPR_DBAT3U 542
#define Gekko_SPR_DBAT3L 543
#define Gekko_SPR_HID0 1008
#define Gekko_SPR_HID1 1009
#define Gekko_SPR_IABR 1010
#define Gekko_SPR_DABR 1013
#define Gekko_SPR_GQRs 912
#define Gekko_SPR_GQR0 912
#define Gekko_SPR_GQR1 913
#define Gekko_SPR_GQR2 914
#define Gekko_SPR_GQR3 915
#define Gekko_SPR_GQR4 916
#define Gekko_SPR_GQR5 917
#define Gekko_SPR_GQR6 918
#define Gekko_SPR_GQR7 919
#define Gekko_SPR_HID2 920
#define Gekko_SPR_WPAR 921
#define Gekko_SPR_DMAU 922
#define Gekko_SPR_DMAL 923

	enum class GekkoRegmode
	{
		GPR = 0,
		FPR,
		PSR,
		MMU
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

	class GekkoRegs : public CuiWindow
	{
		GekkoRegmode mode = GekkoRegmode::GPR;

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

	public:
		GekkoRegs(RECT& rect, std::string name, Cui* parent);
		~GekkoRegs();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}


namespace Debug
{

	class MemoryView : public CuiWindow
	{
		static const size_t RAMSIZE = 0x0180'0000;	// 24 MBytes

		uint32_t cursor = 0x8000'0000;

		std::string hexbyte(uint32_t addr);
		char charbyte(uint32_t addr);

	public:
		MemoryView(RECT& rect, std::string name, Cui* parent);
		~MemoryView();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);

		void SetCursor(uint32_t address);
	};

}



// System-wide debugger

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

