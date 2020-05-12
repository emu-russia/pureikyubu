// DSP ARAM Accelerator

#include "pch.h"

namespace DSP
{
	// Read data by accelerator and optionally decode (raw=false)
	uint16_t DspCore::AccelReadData(bool raw)
	{
		uint16_t val = 0;

		// Check bit15 of ACCAH
		if ((Accel.CurrAddress.h & 0x8000) != 0)
		{
			// This is #UB
			DBHalt("DSP: Accelerator is not configured to read\n");
		}

		if ((Accel.CurrAddress.addr & 0x07ff'ffff) == (Accel.StartAddress.addr & 0x07FF'FFFF))
		{
			if ((Accel.Fmt & 3) != 0 && regs.sr.ge && regs.sr.acie)
			{
				Accel.pendingOverflow = true;
				Accel.overflowVector = DspException::ACR_OVF;
			}
		}

		switch (Accel.Fmt & 3)
		{
			case 0:
				if (Accel.readingSecondNibble)
				{
					Accel.readingSecondNibble = false;
					val = Accel.cachedByte >> 4;
					Accel.CurrAddress.addr += 1;
				}
				else
				{
					Accel.cachedByte = aram.mem[Accel.CurrAddress.addr & 0x07ff'ffff];
					val = Accel.cachedByte & 0xf;
					Accel.readingSecondNibble = true;
				}
				break;

			case 1:
				val = aram.mem[Accel.CurrAddress.addr & 0x07ff'ffff];
				Accel.CurrAddress.addr += 1;
				break;

			case 2:
				val = _byteswap_ushort (*(uint16_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff)));
				Accel.CurrAddress.addr += 2;
				break;

			default:
				DBHalt("DSP: Invalid accelerator mode: 0x%04X\n", Accel.Fmt);
				break;
		}

		// Issue ADPCM Decoder
		if (!raw)
		{
			val = DecodeAdpcm(val);
		}

		if ((Accel.CurrAddress.addr & 0x07ff'ffff) >= (Accel.EndAddress.addr & 0x07FF'FFFF))
		{
			Accel.CurrAddress.addr = Accel.StartAddress.addr;
			if (logAccel)
			{
				DBReport2(DbgChannel::DSP, "Accelerator Overflow while read\n");
			}

			if ((Accel.Fmt & 3) == 0 && regs.sr.ge && regs.sr.acie)
			{
				Accel.pendingOverflow = true;
				Accel.overflowVector = DspException::ADP_OVF;
			}
		}

		return val;
	}

	// Write RAW data to ARAM
	void DspCore::AccelWriteData(uint16_t data)
	{
		// Check bit15 of ACCAH
		if ((Accel.CurrAddress.h & 0x8000) == 0)
		{
			// This is #UB
			DBHalt("DSP: Accelerator is not configured to write\n");
		}

		switch (Accel.Fmt & 3)
		{
			case 1:
				aram.mem[Accel.CurrAddress.addr & 0x07ff'ffff] = (uint8_t)data;
				Accel.CurrAddress.addr += 1;
				break;

			case 2:
				*(uint16_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff)) = _byteswap_ushort(data);
				Accel.CurrAddress.addr += 2;
				break;

			default:
				DBHalt("DSP: Invalid accelerator mode: 0x%04X\n", Accel.Fmt);
				break;
		}

		if ((Accel.CurrAddress.addr & 0x07ff'ffff) >= (Accel.EndAddress.addr & 0x07FF'FFFF))
		{
			Accel.CurrAddress.addr = Accel.StartAddress.addr;
			if (logAccel)
			{
				DBReport2(DbgChannel::DSP, "Accelerator Overflow while write\n");
			}

			if (regs.sr.ge && regs.sr.acie)
			{
				Accel.pendingOverflow = true;
				Accel.overflowVector = DspException::ACW_OVF;
			}
		}
	}

	void DspCore::ResetAccel()
	{
		Accel.readingSecondNibble = false;
		Accel.pendingOverflow = false;
	}

}
