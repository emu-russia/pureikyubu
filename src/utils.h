/*

This section contains common API that have almost atomic significance for all projects.

- Spinlock: Mutually exclusive access synchronization.
- Thread: Portable threads.
- File: File utilities
- String: String utilities
- ByteSwap: Portable byte-swap API

# Note on Strings

The project keeps its text in std::wstring (Unicode code units: UTF-16 on Windows, UTF-32
elsewhere), while every interface that leaves the emulator speaks UTF-8: the Json documents, the
JDI command line and its arguments, the reports, the ImGui/SDL and Win32 front ends.

`WstringToString` and `StringToWstring` are the two directions of that conversion, so a narrow
`std::string` in this code base is UTF-8 and never an ANSI code page. A byte sequence that is not
valid UTF-8 is carried through as the code point of the byte itself rather than dropped, so a name
that came from an unknown source still reaches the file system in one piece.

# Note on Threads

Emulator uses Suspend/Resume methods as control primitives.

The thread procedure is called `Worker`. Unlike conventional implementations, it does not contain an infinite loop, but simply makes one iteration of the thread.
The infinite loop is implemented above (in Thread) to support the Suspend/Resume mechanism, where it is not supported by the native thread implementation (for example, in pthreads).

*/

#pragma once

#if defined(_WINDOWS)

class SpinLock
{
	volatile long _lock = 0;

public:
	void Lock();
	void Unlock();
};

#endif

#ifdef _LINUX

class SpinLock
{
	std::atomic_flag locked = ATOMIC_FLAG_INIT;
public:

	void Lock()
	{
		while (locked.test_and_set(std::memory_order_acquire)) { ; }
	}

	void Unlock()
	{
		locked.clear(std::memory_order_release);
	}
};

#endif


typedef void (*ThreadProc)(void* param);

// A waitable one-shot event: the portable counterpart of the wakeups between the emulator's
// worker threads. Waiting on it blocks the thread, which is the point: a thread that busy-waits
// on a location another core writes makes every write of that location transfer a cache line
// between the cores (and keeps two cores hot), which costs far more than the work the waiting
// thread performs. See the benchmark notes in `testing/gekko_bench`.
class Event
{
#if defined(_WINDOWS)
	HANDLE handle = nullptr;
#else
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool signaled = false;
#endif

public:
	Event();
	~Event();

	// Wake one waiter (or leave the event signaled until somebody waits).
	void Signal();

	// Wait for the event or the timeout. Returns true when it was signaled.
	bool Wait(size_t timeoutMs);
};

class Thread
{
	struct WrappedContext
	{
		ThreadProc proc;
		void* context;
	};

	WrappedContext ctx = { 0 };

	bool running = false;
	SpinLock resumeLock;
	int resumeCounter = 0;
	int suspendCounter = 0;

	std::string threadName;

	// Take care about this place. If it will differ between your projects you get wrecked!

#if defined(_WINDOWS)
	HANDLE threadHandle = INVALID_HANDLE_VALUE;
	DWORD threadId = 0;
	static DWORD WINAPI RingleaderThreadProc(LPVOID lpParameter);
	static const size_t StackSize = 0;
#endif

#if defined(_LINUX)
	pthread_t threadId = 0;
	static void* RingleaderThreadProc(void* args);
	pthread_mutex_t mutex;
	pthread_cond_t cond_var;
	int command;
	bool terminated = false;
#endif

public:

	// Create thread
	Thread(ThreadProc threadProc, bool suspended, void* context, const char* name);

	// Join thread
	~Thread();

	void Resume();
	void Suspend();
	bool IsRunning() { return running; }

	const char* GetName() { return threadName.c_str(); }

	static void Sleep(size_t milliseconds);
};


namespace Util
{
	// Encode wide text as UTF-8.

	std::string WstringToString(const std::wstring& wstr);

	// Decode UTF-8 into wide text.

	std::wstring StringToWstring(const std::string& str);

	// Cursor arithmetic over a UTF-8 string: the byte offset of the code point after (or before)
	// the one that starts at `offset`. The result is clamped to the string, and a byte that cannot
	// start a sequence counts as a code point of its own, so walking a string forwards and then
	// backwards always comes back to where it started, whatever the input is.

	size_t Utf8NextOffset(const std::string& str, size_t offset);
	size_t Utf8PrevOffset(const std::string& str, size_t offset);

	// The code point that starts at `offset`, with `length` set to the number of bytes it takes. A
	// byte that cannot start a sequence is returned as the code point of the byte itself with a
	// length of one, so a caller that walks a string always makes progress and never reads past it.
	// `offset` has to be inside the string (offsets are obtained from Utf8NextOffset/Utf8PrevOffset).

	uint32_t Utf8Codepoint(const std::string& str, size_t offset, size_t& length);

	// Open a file by its (wide) name. fopen() cannot be handed a name that leaves the ANSI code
	// page, so on Windows the call goes through _wfopen_s; elsewhere the name is converted to the
	// UTF-8 the C library expects.

	FILE* FileOpen(const std::wstring& filename, const char* mode);
	FILE* FileOpen(const wchar_t* filename, const char* mode);

	// Get the size of a file.

	size_t FileSize(const std::string& filename);
	size_t FileSize(const std::wstring& filename);
	size_t FileSize(const wchar_t* filename);

	// Check whenever the file exists

	bool FileExists(const std::string& filename);
	bool FileExists(const std::wstring& filename);
	bool FileExists(const wchar_t* filename);

	// Load data from a file

	std::vector<uint8_t> FileLoad(const std::string& filename);
	std::vector<uint8_t> FileLoad(const std::wstring& filename);
	std::vector<uint8_t> FileLoad(const wchar_t* filename);

	// Save data to file

	bool FileSave(const std::string& filename, std::vector<uint8_t>& data);
	bool FileSave(const std::wstring& filename, std::vector<uint8_t>& data);
	bool FileSave(const wchar_t* filename, std::vector<uint8_t>& data);

	// Split a path into its drive, directory, file name and extension. Every destination is passed
	// together with its capacity, so an over-long component (or a UNC prefix, which has no size
	// limit of its own) fails the split instead of overflowing the caller's buffers. On failure the
	// destinations are left empty and false is returned.

	bool SplitPath(const char* _Path,
		char* _Drive, size_t _DriveSize,
		char* _Dir, size_t _DirSize,
		char* _Filename, size_t _FilenameSize,
		char* _Ext, size_t _ExtSize);

	// Get a list of files and directories, relative to the root directory

	void BuildFileTree(std::wstring rootDir, std::list<std::wstring>& names);

	// Check if the entity is a directory or a file.

	bool IsDirectory(std::wstring path);

	// Save a 24-bit RGB image as a PNG file (the GFX screenshots and the unit test report use it).
	// `rgb` holds width * height triplets, top row first.

	bool SavePng(const char* filename, const uint8_t* rgb, size_t width, size_t height);

}

#if defined(_WINDOWS)

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


#ifdef _LINUX
#define CNTLZ(mask) __builtin_clz(mask)
#else
#define CNTLZ(mask) __lzcnt(mask)
#endif
