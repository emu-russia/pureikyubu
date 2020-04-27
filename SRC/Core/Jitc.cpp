
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
		auto it = segments.find(addr);

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

		size_t maxInstructions = 0x100;

		Prolog(segment);

		while (maxInstructions--)
		{
			uint32_t physicalAddress = core->EffectiveToPhysical(addr, true);
			uint32_t instr;

			if (physicalAddress == -1)
			{
				// ISI
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

		segments[segment->addr] = segment;

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
					delete it->second;
					it->second = nullptr;
					continue;
				}

				if (seg->addr >= addr && addr < (seg->addr + seg->size))
				{
					delete it->second;
					it->second = nullptr;
					continue;
				}
			}
		}
	}

	void Jitc::Execute()
	{
		CodeSegment* segment = SegmentCompiled(core->regs.pc);
		if (segment == nullptr)
		{
			segment = CompileSegment(core->regs.pc);
		}
		assert(segment);

		segment->Run();

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

	void Jitc::Reset()
	{
		InvalidateAll();
	}

	void Jitc::CompileInstr(AnalyzeInfo* info, CodeSegment* segment)
	{
		switch (info->instr)
		{
			default:
				FallbackStub(info, segment);
				break;
		}
	}

}
