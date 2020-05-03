#include "pch.h"

namespace Gekko
{
	Jitc::Jitc(GekkoCore* _core)
	{
		core = _core;
	}

	Jitc::~Jitc()
	{
		InvalidateAll();
	}

	CodeSegment* Jitc::SegmentCompiled(uint32_t addr)
	{
		auto it = segments.find(addr >> 2);

		if (it != segments.end())
		{
			return it->second;
		}

		return nullptr;
	}

	CodeSegment* Jitc::CompileSegment(uint32_t addr)
	{
		AnalyzeInfo info = { 0 };
		CodeSegment* segment = new CodeSegment();

		segment->addr = addr;
		segment->code.reserve(0x100);
		segment->core = core;

		// Usually this is enough, but if the segment is larger, nothing bad will happen, it will just break into several parts.

		size_t maxInstructions = 0x100;

		Prolog(segment);

		while (maxInstructions--)
		{
			if (core->TestBreakpointForJitc(addr))
				break;

			uint32_t physicalAddress = core->EffectiveToPhysical(addr, MmuAccess::Execute);
			uint32_t instr;

			// TODO: This moment needs to be rethinked. It makes little sense to generate ISI in the compilation process, 
			// you need to do this in the process of executing compiled code.
			// Most likely you need to inject code to generate ISI and break segment compilation.

			if (physicalAddress == BadAddress)
			{
				core->Exception(Exception::ISI);
				break;
			}

			MIReadWord(physicalAddress, &instr);

			Analyzer::AnalyzeFast(addr, instr, &info);

			CompileInstr(&info, segment);

			addr += 4;
			segment->size += 4;

			if (info.flow)
				break;
		}

		Epilog(segment);

		segments[segment->addr >> 2] = segment;

		DWORD notNeeded;
		VirtualProtect(segment->code.data(), segment->code.size(), PAGE_EXECUTE_READWRITE, &notNeeded);

		return segment;
	}

	void CodeSegment::Run()
	{
		//DBReport2(DbgChannel::CPU, "Run code segment: 0x%08X, segs: %i\n", addr, core->segmentsExecuted);

		void (*codePtr)() = (void (*)())code.data();
		codePtr();

		core->segmentsExecuted++;
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

	void Jitc::InvalidateAll()
	{
		for (auto it = segments.begin(); it != segments.end(); ++it)
		{
			if (it->second)
			{
				if (it->second == currentSegment)
				{
					continue;
				}

				delete it->second;
				it->second = nullptr;
			}
		}
		segments.clear();
	}

	void Jitc::Invalidate(uint32_t addr, size_t size)
	{
		for (auto it = segments.begin(); it != segments.end(); ++it)
		{
			CodeSegment* seg = it->second;
			if (seg != nullptr)
			{
				// If a invalidated region crosses a segment somehow, invalidate the entire segment.

				if (addr >= seg->addr && seg->addr < (addr + size))
				{
					if (it->second != currentSegment)
					{
						delete it->second;
						it->second = nullptr;
						continue;
					}
				}

				if (seg->addr >= addr && addr < (seg->addr + seg->size))
				{
					if (it->second != currentSegment)
					{
						delete it->second;
						it->second = nullptr;
						continue;
					}
				}
			}
		}
	}

	void Jitc::Execute()
	{
		currentSegment = SegmentCompiled(core->regs.pc);
		if (currentSegment == nullptr)
		{
			currentSegment = CompileSegment(core->regs.pc);
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
	}

	void Jitc::Reset()
	{
		InvalidateAll();
	}

	void Jitc::CompileInstr(AnalyzeInfo* info, CodeSegment* seg)
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
			case Instruction::lwz: LoadImm(info, seg, ReadWord); break;

			case Instruction::rlwinm: Rlwinm(info, seg); break;

			default:
				FallbackStub(info, seg);
				break;
		}
	}

}
