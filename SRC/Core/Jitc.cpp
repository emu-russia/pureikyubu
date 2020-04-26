
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

		size_t maxInstructions = 0x100;

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

		segments[addr] = segment;

		return segment;
	}

	void CodeSegment::Run()
	{
		void (*codePtr)() = (void (*)())code.data();
		codePtr();
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
