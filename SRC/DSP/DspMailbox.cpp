// DSP Mailbox processing

#include "pch.h"

using namespace Debug;

namespace DSP
{
	// CPU->DSP Mailbox

	// Write by processor only.

	void DspCore::CpuToDspWriteHi(uint16_t value)
	{
		_TB(DspCore::CpuToDspWriteHi);
		CpuToDspLock[0].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspWriteHi: 0x%04X\n", value);
		}

		if (CpuToDspMailbox[0] & 0x8000)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "CPU Message discarded.\n");
			}
		}

		CpuToDspMailbox[0] = value & 0x7FFF;
		CpuToDspLock[0].Unlock();
		_TE();
	}

	void DspCore::CpuToDspWriteLo(uint16_t value)
	{
		_TB(DspCore::CpuToDspWriteLo);
		CpuToDspLock[1].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "CpuToDspWriteLo: 0x%04X\n", value);
		}

		CpuToDspMailbox[1] = value;
		CpuToDspMailbox[0] |= 0x8000;

		if (logMailbox)
		{
			Report(Channel::DSP, "CPU Write Message: 0x%04X_%04X\n", CpuToDspMailbox[0], CpuToDspMailbox[1]);
		}
		CpuToDspLock[1].Unlock();
		_TE();
	}

	uint16_t DspCore::CpuToDspReadHi(bool ReadByDsp)
	{
		CpuToDspLock[0].Lock();
		uint16_t value = CpuToDspMailbox[0];
		CpuToDspLock[0].Unlock();

		// TODO:

		// If DSP is running and is in a waiting cycle for a message from the CPU, 
		// we put it in the HALT state until the processor sends a message through the Mailbox.

		//if ((value & 0x8000) == 0 && IsRunning() && ReadByDsp)
		//{
		//	DBReport2(DbgChannel::DSP, "Wait CPU Mailbox\n");
		//	Suspend();
		//}

		return value;
	}

	uint16_t DspCore::CpuToDspReadLo(bool ReadByDsp)
	{
		CpuToDspLock[1].Lock();
		uint16_t value = CpuToDspMailbox[1];
		if (ReadByDsp)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "DSP Read Message: 0x%04X_%04X\n", CpuToDspMailbox[0], CpuToDspMailbox[1]);
			}
			CpuToDspMailbox[0] &= ~0x8000;				// When DSP read
		}
		CpuToDspLock[1].Unlock();
		return value;
	}

	// DSP->CPU Mailbox

	// Write by DSP only.

	void DspCore::DspToCpuWriteHi(uint16_t value)
	{
		_TB(DspCore::DspToCpuWriteHi);
		DspToCpuLock[0].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "DspToCpuWriteHi: 0x%04X\n", value);
		}

		if (DspToCpuMailbox[0] & 0x8000)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "DSP Message discarded.\n");
			}
		}

		DspToCpuMailbox[0] = value & 0x7FFF;
		DspToCpuLock[0].Unlock();
		_TE();
	}

	void DspCore::DspToCpuWriteLo(uint16_t value)
	{
		_TB(DspCore::DspToCpuWriteLo);
		DspToCpuLock[1].Lock();

		if (logInsaneMailbox)
		{
			Report(Channel::DSP, "DspToCpuWriteLo: 0x%04X\n", value);
		}

		DspToCpuMailbox[1] = value;
		DspToCpuMailbox[0] |= 0x8000;

		if (logMailbox)
		{
			Report(Channel::DSP, "DSP Write Message: 0x%04X_%04X\n", DspToCpuMailbox[0], DspToCpuMailbox[1]);
		}

		DspToCpuLock[1].Unlock();
		_TE();
	}

	uint16_t DspCore::DspToCpuReadHi(bool ReadByDsp)
	{
		DspToCpuLock[0].Lock();
		uint16_t value = DspToCpuMailbox[0];
		DspToCpuLock[0].Unlock();

		return value;
	}

	uint16_t DspCore::DspToCpuReadLo(bool ReadByDsp)
	{
		DspToCpuLock[1].Lock();
		uint16_t value = DspToCpuMailbox[1];
		if (!ReadByDsp)
		{
			if (logMailbox)
			{
				Report(Channel::DSP, "CPU Read Message: 0x%04X_%04X\n", DspToCpuMailbox[0], DspToCpuMailbox[1]);
			}
			DspToCpuMailbox[0] &= ~0x8000;					// When CPU read
		}
		DspToCpuLock[1].Unlock();
		return value;
	}
}
