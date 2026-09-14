// The pieces gba_types.h declares but does not define: the log sink and the memory banks.

#include "gba_types.h"

#include <cstdarg>

namespace GBA
{
	static LogSink logSink = nullptr;
	static void* logUser = nullptr;

	void SetLogSink(LogSink sink, void* user)
	{
		logSink = sink;
		logUser = user;
	}

	void Log(LogLevel level, const char* format, ...)
	{
		if (logSink == nullptr)
		{
			return;
		}

		char text[1024];

		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);

		logSink(level, text, logUser);
	}

	void MemoryBank::Init(u32 bytes)
	{
		Free();

		size = bytes;
		powerOfTwo = (bytes != 0) && ((bytes & (bytes - 1)) == 0);
		mask = powerOfTwo ? (bytes - 1) : 0;
		storage = new u8[bytes ? bytes : 1];
		memset(storage, 0, bytes ? bytes : 1);
	}

	void MemoryBank::Free()
	{
		delete[] storage;
		storage = nullptr;
		size = 0;
		mask = 0;
		powerOfTwo = true;
	}
}
