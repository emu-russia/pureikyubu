// DSP ARAM Accelerator

#include "pch.h"

namespace DSP
{
	uint16_t DspCore::AccelFetch()
	{
		uint16_t val = 0;
		uint8_t tempByte = 0;

		switch (Accel.Fmt & 3)
		{
			case 0:

				// Refresh pred/scale
				if ((Accel.CurrAddress.addr & 0xF) == 0 && ((Accel.Fmt >> 2) & 3) == 0)
				{
					Accel.AdpcmPds = *(uint8_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff) / 2);
					Accel.CurrAddress.addr += 2;
				}

				tempByte = *(uint8_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff) / 2);
				if ((Accel.CurrAddress.addr & 1) == 0)
				{
					val = tempByte >> 4;
				}
				else
				{
					val = tempByte & 0xf;
				}
				break;

			case 1:
				val = *(uint8_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff));
				break;

			case 2:
				val = _byteswap_ushort(*(uint16_t*)(aram.mem + 2 * (Accel.CurrAddress.addr & 0x07ff'ffff)));
				break;

			default:
				DBHalt("DSP: Invalid accelerator mode: 0x%04X\n", Accel.Fmt);
				break;
		}

		Accel.CurrAddress.addr++;

		if ((Accel.CurrAddress.addr & 0x07ff'ffff) >= (Accel.EndAddress.addr & 0x07FF'FFFF))
		{
			Accel.CurrAddress.addr = Accel.StartAddress.addr;
			if (logAccel)
			{
				DBReport2(DbgChannel::DSP, "Accelerator Overflow while read\n");
			}

			if (regs.sr.ge && regs.sr.acie)
			{
				Accel.pendingOverflow = true;
				Accel.overflowVector = ((Accel.Fmt >> 3) & 3) == 0 ? DspException::ADP_OVF : DspException::ACR_OVF;
			}
		}

		return val;
	}

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

		val = AccelFetch();

		// Issue ADPCM Decoder
		if (!raw)
		{
			val = DecodeAdpcm(val);
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

		// Write mode is always 16-bit

		*(uint16_t*)(aram.mem + 2 * (Accel.CurrAddress.addr & 0x07ff'ffff)) = _byteswap_ushort(data);
		Accel.CurrAddress.addr++;

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
		Accel.pendingOverflow = false;
	}

}
