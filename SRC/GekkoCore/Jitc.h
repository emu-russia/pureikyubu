// Gekko recompiler.

#pragma once

namespace Gekko
{
	class CodeSegment
	{
	public:
		GekkoCore* core = nullptr;	// Parent core
		uint32_t addr = 0;		// Starting Gekko code address (effective)
		size_t size = 0;		// Size of Gekko code in bytes
		std::vector<uint8_t> code;	  // Recompiled code, automatically inflates when necessary

		void Run();

		void Write8(uint8_t data);
		void Write16(uint16_t data);
		void Write32(uint32_t data);
		void Write64(uint64_t data);
		void Write(const IntelCore::DecoderInfo& info);
	};

	class Jitc
	{
		friend JitCommands;
		friend GekkoCoreUnitTest::GekkoCoreUnitTest;

		GekkoCore* core;		// Saved instance of the parent core

		std::unordered_map<uint32_t, CodeSegment*> segments;

		CodeSegment* SegmentCompiled(uint32_t addr);
		CodeSegment* CompileSegment(uint32_t addr);
		void CompileInstr(DecoderInfo* info, CodeSegment* segment);

		void InvalidateAll();

		// Gekko ISA

		void Prolog(CodeSegment* seg);
		void Epilog(CodeSegment* seg);
		size_t EpilogSize();
		void AddPc(CodeSegment* seg);
		void CallTick(CodeSegment* seg);

		typedef void(__FASTCALL* LoadDelegate)(uint32_t addr, uint32_t* reg);
		typedef void(__FASTCALL* StoreDelegate)(uint32_t addr, uint32_t* reg);

		void FallbackStub(DecoderInfo* info, CodeSegment* seg);

		void Add(DecoderInfo* info, CodeSegment* seg);
		void Addd(DecoderInfo* info, CodeSegment* seg);
		void Addo(DecoderInfo* info, CodeSegment* seg);
		void Addod(DecoderInfo* info, CodeSegment* seg);

		void Branch(DecoderInfo* info, CodeSegment* seg, bool link);

		void LoadImm(DecoderInfo* info, CodeSegment* seg, LoadDelegate loadProc);

		void Rlwinm(DecoderInfo* info, CodeSegment* seg);

		void PsAdd(DecoderInfo* info, CodeSegment* seg);
		void PsSub(DecoderInfo* info, CodeSegment* seg);
		void PsMerge00(DecoderInfo* info, CodeSegment* seg);
		void PsMerge01(DecoderInfo* info, CodeSegment* seg);
		void PsMerge10(DecoderInfo* info, CodeSegment* seg);
		void PsMerge11(DecoderInfo* info, CodeSegment* seg);

		void Dequantize(CodeSegment* seg, void* psReg, GEKKO_QUANT_TYPE type, uint8_t scale, bool secondReg);
		void PSQLoad(DecoderInfo* info, CodeSegment* seg);

		// These methods require fastcall, as they are called from recompiled code.

		static void __FASTCALL ReadByte(uint32_t addr, uint32_t* reg) { Gekko->ReadByte(addr, reg); }
		static void __FASTCALL WriteByte(uint32_t addr, uint32_t data) { Gekko->WriteByte(addr, data); }
		static void __FASTCALL ReadHalf(uint32_t addr, uint32_t* reg) { Gekko->ReadHalf(addr, reg); }
		static void __FASTCALL WriteHalf(uint32_t addr, uint32_t data) { Gekko->WriteHalf(addr, data); }
		static void __FASTCALL ReadWord(uint32_t addr, uint32_t* reg) { Gekko->ReadWord(addr, reg); }
		static void __FASTCALL WriteWord(uint32_t addr, uint32_t data) { Gekko->WriteWord(addr, data); }
		static void __FASTCALL ReadDouble(uint32_t addr, uint64_t* reg) { Gekko->ReadDouble(addr, reg); }
		static void __FASTCALL WriteDouble(uint32_t addr, uint64_t* data) { Gekko->WriteDouble(addr, data); }

		static bool ExecuteInterpeterFallback();
		static void Tick();

		// This segment is not involved in invalidation, so as not to disrupt the code.
		CodeSegment* currentSegment = nullptr;

		uint32_t DequantizeTemp = 0;

		// Usually this is enough, but if the segment is larger, nothing bad will happen, it will just break into several parts.
		size_t maxInstructions = 0x100;

	public:
		Jitc(GekkoCore* _core);
		~Jitc();

		void Invalidate(uint32_t addr, size_t size);

		void Execute();
		void Reset();
	};

}
