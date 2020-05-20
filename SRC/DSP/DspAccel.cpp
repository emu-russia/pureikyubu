// DSP ARAM Accelerator

// Accelerator addresses work in accordance with the selected mode (4-bit, 8-bit, 16-bit). 
// That is, for example, in 4-bit mode - the start, current and end addresses point to nibble in ARAM.

// On a real system, the next piece of data is cached in 3 16-bit registers ("output ports"). 
// Here we do not repeat this mechanism, but refer directly to ARAM.

// The accelerator can work both independently, simply driving data between ARAM and DSP, and in conjunction with an ADPCM decoder.
// In the first case, one register is used (ACDAT, read-write), in the second case, another (ACDAT2, read-only).

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

				// TODO: Check currAddr == endAddr after Pred/Scale update.

				tempByte = *(uint8_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff) / 2);
				if ((Accel.CurrAddress.addr & 1) == 0)
				{
					val = tempByte >> 4;		// High nibble
				}
				else
				{
					val = tempByte & 0xf;		// Low nibble
				}
				break;

			case 1:
				val = *(uint8_t*)(aram.mem + (Accel.CurrAddress.addr & 0x07ff'ffff));
				break;

			case 2:
				val = _byteswap_ushort(*(uint16_t*)(aram.mem + 2 * (uint64_t)(Accel.CurrAddress.addr & 0x07ff'ffff)));
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

		*(uint16_t*)(aram.mem + 2 * (uint64_t)(Accel.CurrAddress.addr & 0x07ff'ffff)) = _byteswap_ushort(data);
		Accel.CurrAddress.addr++;

		if ((Accel.CurrAddress.addr & 0x07ff'ffff) >= (Accel.EndAddress.addr & 0x07FF'FFFF))
		{
			Accel.CurrAddress.addr = Accel.StartAddress.addr;
			Accel.CurrAddress.h |= 0x8000;
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
