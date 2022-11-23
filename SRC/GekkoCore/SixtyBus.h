
#pragma once

namespace Gekko
{
	void SixtyBus_ReadByte(uint32_t phys_addr, uint32_t* reg);
	void SixtyBus_WriteByte(uint32_t phys_addr, uint32_t data);
	void SixtyBus_ReadHalf(uint32_t phys_addr, uint32_t* reg);
	void SixtyBus_WriteHalf(uint32_t phys_addr, uint32_t data);
	void SixtyBus_ReadWord(uint32_t phys_addr, uint32_t* reg);
	void SixtyBus_WriteWord(uint32_t phys_addr, uint32_t data);
	void SixtyBus_ReadDouble(uint32_t phys_addr, uint64_t* reg);
	void SixtyBus_WriteDouble(uint32_t phys_addr, uint64_t* data);
	void SixtyBus_ReadBurst(uint32_t phys_addr, uint8_t burstData[32]);
	void SixtyBus_WriteBurst(uint32_t phys_addr, uint8_t burstData[32]);
}
