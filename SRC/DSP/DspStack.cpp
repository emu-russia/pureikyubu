// DSPcore contains a built-in stack implementation.
#include "pch.h"

namespace DSP
{

	DspStack::DspStack(size_t _depth)
	{
		depth = (int)_depth;
		stack = new uint16_t[depth];
	}

	DspStack::~DspStack()
	{
		delete[] stack;
	}

	bool DspStack::push(uint16_t val)
	{
		if (ptr >= depth)
			return false;	// Overflow

		stack[ptr++] = val;
		return true;
	}

	bool DspStack::pop(uint16_t& val)
	{
		if (ptr == 0)
			return false;	// Underflow

		val = stack[--ptr];
		return true;
	}

	uint16_t DspStack::top()
	{
		return stack[ptr - 1];
	}

	uint16_t DspStack::at(int pos)
	{
		return stack[pos];
	}

	bool DspStack::empty()
	{
		return ptr == 0;
	}

	int DspStack::size()
	{
		return ptr;
	}

	void DspStack::clear()
	{
		ptr = 0;
	}

}
