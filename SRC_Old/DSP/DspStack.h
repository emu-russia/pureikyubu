// DSPcore stack implementation.

#pragma once

namespace DSP
{

	class DspStack
	{
		uint16_t* stack;
		int ptr = 0;
		int depth;

	public:
		DspStack(size_t _depth);
		~DspStack();

		bool push(uint16_t val);
		bool pop(uint16_t& val);
		uint16_t top();
		uint16_t at(int pos);
		bool empty();
		int size();
		void clear();
	};

}
