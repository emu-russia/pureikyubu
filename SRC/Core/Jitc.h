
#pragma once

#include <map>
#include <vector>

namespace Gekko
{
	class CodeSegment
	{
	public:
		uint32_t addr;
		size_t size;
		std::vector<uint8_t> code;
	};

	class Jitc
	{
		GekkoCore* core;		// Saved instance of the parent core

		std::map<uint32_t, CodeSegment*> segments;

	public:
		Jitc(GekkoCore* _core);
		~Jitc();

		bool SegmentCompiled(uint32_t addr);
		void CompileSegment(uint32_t addr);

		void RunSegment(uint32_t addr);

		void Invalidate(uint32_t addr, size_t size);

	};

}
