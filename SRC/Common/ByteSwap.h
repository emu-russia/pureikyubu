
#pragma once

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)

#include <intrin.h>

#define _BYTESWAP_UINT16 _byteswap_ushort
#define _BYTESWAP_UINT32 _byteswap_ulong
#define _BYTESWAP_UINT64 _byteswap_uint64

#endif

#if defined(_LINUX)

#include <byteswap.h>

#define _BYTESWAP_UINT16 __bswap_16
#define _BYTESWAP_UINT32 __bswap_32
#define _BYTESWAP_UINT64 __bswap_64

#endif
