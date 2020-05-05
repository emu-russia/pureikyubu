// EFB - embedded framebuffer reads (peeks) and writes (pokes)
#include "pch.h"

void EFBPeek8(uint32_t ofs, uint32_t *reg)
{
	DBReport2(DbgChannel::GP, "EFBPeek8: 0x%08X\n", ofs);
}

void EFBPeek16(uint32_t ofs, uint32_t *reg)
{
	DBReport2(DbgChannel::GP, "EFBPeek16: 0x%08X\n", ofs);
}

void EFBPeek32(uint32_t ofs, uint32_t *reg)
{
	DBReport2(DbgChannel::GP, "EFBPeek32: 0x%08X\n", ofs);
}

// ---------------------------------------------------------------------------

void EFBPoke8(uint32_t ofs, uint32_t data)
{
	DBReport2(DbgChannel::GP, "EFBPoke8: 0x%08X\n", ofs);
}

void EFBPoke16(uint32_t ofs, uint32_t data)
{
	DBReport2(DbgChannel::GP, "EFBPoke16: 0x%08X\n", ofs);
}

void EFBPoke32(uint32_t ofs, uint32_t data)
{
	DBReport2(DbgChannel::GP, "EFBPoke32: 0x%08X\n", ofs);
}
