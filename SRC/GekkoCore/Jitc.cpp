#include "pch.h"

namespace Gekko
{
	typedef CodeSegment* CodeSegmentPtr;

	Jitc::Jitc(GekkoCore* _core)
	{
		core = _core;
		segments = new CodeSegment * [RAMSIZE >> 2];
		memset(segments, 0, (RAMSIZE >> 2) * sizeof(CodeSegment*));
	}

	Jitc::~Jitc()
	{
		InvalidateAll();
		delete segments;
	}

	CodeSegment* Jitc::SegmentCompiled(uint32_t physicalAddr)
	{
		return segments[physicalAddr >> 2];
	}

	CodeSegment* Jitc::CompileSegment(uint32_t physicalAddr, uint32_t virtualAddr)
	{
		DecoderInfo info = { 0 };
		CodeSegment* segment = new CodeSegment();

		uint32_t pa = physicalAddr;

		segment->addr = pa;
		segment->code.reserve(totalSegments ? totalSegmentBytes / totalSegments : 0x20);
		segment->core = core;

		Prolog(segment);

		size_t n = maxInstructions;

		while (n--)
		{
			// TODO:
			//if (core->TestBreakpointForJitc(addr))
			//	break;

			// Interrupt the translation at the boundary of the page.
			if ((pa & ~0xfff) != (physicalAddr & ~0xfff))
				break;

			uint32_t instr;
			PIReadWord(pa, &instr);

			Decoder::DecodeFast(virtualAddr, instr, &info);

			CompileInstr(&info, segment);

			pa += 4;
			virtualAddr += 4;
			segment->size += 4;

			if (info.flow)
				break;
		}

		Epilog(segment);

		segments[segment->addr >> 2] = segment;

		totalSegmentBytes += segment->size;
		totalSegments++;

#ifdef _WINDOWS
		DWORD notNeeded;
		VirtualProtect(segment->code.data(), segment->code.size(), PAGE_EXECUTE_READWRITE, &notNeeded);
#endif

#ifdef _LINUX
		// TODO
#endif

		core->compiledSegments++;

		return segment;
	}

	void CodeSegment::Run()
	{
		//DBReport2(DbgChannel::CPU, "Run code segment: 0x%08X, segs: %i\n", addr, core->segmentsExecuted);

		void (*codePtr)() = (void (*)())code.data();
		codePtr();

		core->executedSegments++;
	}

	void CodeSegment::Write8(uint8_t data)
	{
		code.push_back(data);
	}

	void CodeSegment::Write16(uint16_t data)
	{
		Write8((uint8_t)data);
		Write8((uint8_t)(data >> 8));
	}

	void CodeSegment::Write32(uint32_t data)
	{
		Write16((uint16_t)data);
		Write16((uint16_t)(data >> 16));
	}

	void CodeSegment::Write64(uint64_t data)
	{
		Write32((uint32_t)data);
		Write32((uint32_t)(data >> 32));
	}

	void CodeSegment::Write(const IntelCore::DecoderInfo& info)
	{
		for (size_t i = 0; i < info.prefixSize; i++)
		{
			code.push_back(info.prefixBytes[i]);
		}

		for (size_t i = 0; i < info.instrSize; i++)
		{
			code.push_back(info.instrBytes[i]);
		}
	}

	void Jitc::InvalidateAll()
	{
		size_t segNum = RAMSIZE >> 2;
		for (size_t n = 0; n < segNum; n++)
		{
			if (segments[n] == currentSegment)
			{
				continue;
			}

			delete segments[n];
			segments[n] = nullptr;
		}
	}

	void Jitc::Invalidate(uint32_t addr, size_t size)
	{
		size_t segStart = addr >> 2;
		size_t segEnd = (addr + size + 4) >> 2;
		for (size_t n = segStart; n < segEnd; n++)
		{
			CodeSegment* seg = segments[n];
			if (seg != nullptr)
			{
				// If a invalidated region crosses a segment somehow, invalidate the entire segment.

				if (addr >= seg->addr && seg->addr < (addr + size))
				{
					if (seg != currentSegment)
					{
						delete seg;
						segments[n] = nullptr;
						continue;
					}
				}

				if (seg->addr >= addr && addr < (seg->addr + seg->size))
				{
					if (seg != currentSegment)
					{
						delete seg;
						segments[n] = nullptr;
						continue;
					}
				}
			}
		}
	}

	void Jitc::Execute()
	{
		int WIMG;
		uint32_t physicalAddress = core->EffectiveToPhysical(core->regs.pc, MmuAccess::Execute, WIMG);

		if (physicalAddress == BadAddress)
		{
			core->Exception(Exception::ISI);
			return;
		}

		currentSegment = SegmentCompiled(physicalAddress);
		if (currentSegment == nullptr)
		{
			currentSegment = CompileSegment(physicalAddress, core->regs.pc);
		}
		assert(currentSegment);

		currentSegment->Run();

		// Branch-specific checks

		if (!core->exception)
		{
			if (core->intFlag && (core->regs.msr & MSR_EE))
			{
				core->Exception(Gekko::Exception::INTERRUPT);
				return;
			}

			if (core->decreq && (core->regs.msr & MSR_EE))
			{
				core->decreq = false;
				core->Exception(Gekko::Exception::DECREMENTER);
			}
		}

		core->exception = false;

		if (core->resetInstructionCounter)
		{
			core->resetInstructionCounter = false;
			core->ops = 0;
		}
	}

	bool Jitc::ExecuteInterpeterFallback()
	{
		return Gekko->interp->ExecuteInterpeterFallback();
	}

	void Jitc::Reset()
	{
		InvalidateAll();
	}

	void Jitc::Tick()
	{
		Gekko->Tick();
		Gekko->ops++;
	}

	void Jitc::CompileInstr(DecoderInfo* info, CodeSegment* seg)
	{
		switch (info->instr)
		{
			case Instruction::add: Add(info, seg); break;
			//case Instruction::add_d: Addd(info, seg); break;
			//case Instruction::addo: Addo(info, seg); break;
			//case Instruction::addo_d: Addod(info, seg); break;

			case Instruction::b: Branch(info, seg, false); break;
			case Instruction::ba: Branch(info, seg, false); break;
			case Instruction::bl: Branch(info, seg, true); break;
			case Instruction::bla: Branch(info, seg, true); break;

			case Instruction::lbz: LoadImm(info, seg, ReadByte); break;
			case Instruction::lhz: LoadImm(info, seg, ReadHalf); break;
			case Instruction::lwz: LoadImm(info, seg, ReadWord); break;

			case Instruction::rlwinm: Rlwinm(info, seg); break;

			case Instruction::ps_add: PsAdd(info, seg); break;
			case Instruction::ps_sub: PsSub(info, seg); break;
			case Instruction::ps_merge00: PsMerge00(info, seg); break;
			case Instruction::ps_merge01: PsMerge01(info, seg); break;
			case Instruction::ps_merge10: PsMerge10(info, seg); break;
			case Instruction::ps_merge11: PsMerge11(info, seg); break;

			//case Instruction::psq_l: PSQLoad(info, seg); break;

			default:
#if GEKKOCORE_JITC_HALT_ON_UNIMPLEMENTED_OPCODE
				Debug::Halt("Unimplemented opcode %s\n", Gekko::GekkoDisasm::InstrToString(info).c_str());
#else
				FallbackStub(info, seg);
#endif
				break;
		}
	}

}
