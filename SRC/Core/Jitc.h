// Gekko recompiler.

#pragma once

#include <unordered_map>
#include <vector>

namespace Gekko
{
	class CodeSegment
	{
	public:
		GekkoCore* core;		// Parent core
		uint32_t addr = 0;		// Starting Gekko code address (effective)
		size_t size = 0;		// Size of Gekko code in bytes
		std::vector<uint8_t> code;	  // Automatically inflates when necessary

		void Run();

		void Write8(uint8_t data);
		void Write16(uint16_t data);
		void Write32(uint32_t data);
		void Write64(uint64_t data);
	};

	class Jitc
	{
		GekkoCore* core;		// Saved instance of the parent core

		std::unordered_map<uint32_t, CodeSegment*> segments;

		CodeSegment* SegmentCompiled(uint32_t addr);
		CodeSegment* CompileSegment(uint32_t addr);
		void CompileInstr(AnalyzeInfo* info, CodeSegment* segment);

		void InvalidateAll();

		// Gekko ISA

		void Prolog(CodeSegment* seg);
		void Epilog(CodeSegment* seg);
		size_t EpilogSize();
		void AddPc(CodeSegment* seg);
		void CallTick(CodeSegment* seg);

		typedef void(__fastcall* LoadDelegate)(uint32_t addr, uint32_t* reg);
		typedef void(__fastcall* StoreDelegate)(uint32_t addr, uint32_t* reg);

		void FallbackStub(AnalyzeInfo* info, CodeSegment* seg);

		void Add(AnalyzeInfo* info, CodeSegment* seg);
		void Addd(AnalyzeInfo* info, CodeSegment* seg);
		void Addo(AnalyzeInfo* info, CodeSegment* seg);
		void Addod(AnalyzeInfo* info, CodeSegment* seg);

		void Branch(AnalyzeInfo* info, CodeSegment* seg, bool link);

		void LoadImm(AnalyzeInfo* info, CodeSegment* seg, LoadDelegate loadProc);

		void Rlwinm(AnalyzeInfo* info, CodeSegment* seg);

		void PsAdd(AnalyzeInfo* info, CodeSegment* seg);
		void PsSub(AnalyzeInfo* info, CodeSegment* seg);
		void PsMerge00(AnalyzeInfo* info, CodeSegment* seg);
		void PsMerge01(AnalyzeInfo* info, CodeSegment* seg);
		void PsMerge10(AnalyzeInfo* info, CodeSegment* seg);
		void PsMerge11(AnalyzeInfo* info, CodeSegment* seg);

		void Dequantize(CodeSegment* seg, void* psReg, GEKKO_QUANT_TYPE type, uint8_t scale, bool secondReg);
		void PSQLoad(AnalyzeInfo* info, CodeSegment* seg);

		// These methods require __fastacall, as they are called from recompiled code.

		static void __fastcall ReadByte(uint32_t addr, uint32_t* reg) { Gekko->ReadByte(addr, reg); }
		static void __fastcall WriteByte(uint32_t addr, uint32_t data) { Gekko->WriteByte(addr, data); }
		static void __fastcall ReadHalf(uint32_t addr, uint32_t* reg) { Gekko->ReadHalf(addr, reg); }
		static void __fastcall WriteHalf(uint32_t addr, uint32_t data) { Gekko->WriteHalf(addr, data); }
		static void __fastcall ReadWord(uint32_t addr, uint32_t* reg) { Gekko->ReadWord(addr, reg); }
		static void __fastcall WriteWord(uint32_t addr, uint32_t data) { Gekko->WriteWord(addr, data); }
		static void __fastcall ReadDouble(uint32_t addr, uint64_t* reg) { Gekko->ReadDouble(addr, reg); }
		static void __fastcall WriteDouble(uint32_t addr, uint64_t* data) { Gekko->WriteDouble(addr, data); }

		static bool ExecuteInterpeterFallback();
		static void Tick();

		// This segment is not involved in invalidation, so as not to disrupt the code.
		CodeSegment* currentSegment = nullptr;

		uint32_t DequantizeTemp = 0;

	public:
		Jitc(GekkoCore* _core);
		~Jitc();

		void Invalidate(uint32_t addr, size_t size);

		void Execute();
		void Reset();
	};

}
