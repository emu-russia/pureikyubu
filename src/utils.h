
#pragma once

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)

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

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)
#include <windows.h>
#endif

typedef void (*ThreadProc)(void* param);

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

#if defined(_WINDOWS) || defined(_PLAYGROUND_WINDOWS)
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

	static void Sleep(size_t milliseconds);
};


namespace Util
{
    std::string WstringToString(const std::wstring& wstr);

    std::wstring StringToWstring(const std::string& str);

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

    void SplitPath(const char* _Path,
        char* _Drive,
        char* _Dir,
        char* _Filename,
        char* _Ext);

    // Get a list of files and directories, relative to the root directory

    void BuildFileTree(std::wstring rootDir, std::list<std::wstring>& names);

    // Check if the entity is a directory or a file.

    bool IsDirectory(std::wstring path);

}

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
