#include "pch.h"

#ifdef _WINDOWS

void SpinLock::Lock()
{
	while (_InterlockedCompareExchange(&_lock,
		1, // exchange
		0)  // comparand
		== 1)
	{
		// spin!
		_mm_pause();
	}
}

void SpinLock::Unlock()
{
	_InterlockedExchange(&_lock, 0);
}

#endif

Event::Event()
{
#if defined(_WINDOWS)
	handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(handle != nullptr);
#else
	pthread_mutex_init(&mutex, nullptr);
	pthread_cond_init(&cond, nullptr);
	signaled = false;
#endif
}

Event::~Event()
{
#if defined(_WINDOWS)
	if (handle != nullptr)
	{
		CloseHandle(handle);
		handle = nullptr;
	}
#else
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
#endif
}

void Event::Signal()
{
#if defined(_WINDOWS)
	SetEvent(handle);
#else
	pthread_mutex_lock(&mutex);
	signaled = true;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
#endif
}

bool Event::Wait(size_t timeoutMs)
{
#if defined(_WINDOWS)
	return WaitForSingleObject(handle, (DWORD)timeoutMs) == WAIT_OBJECT_0;
#else
	struct timespec until;
	clock_gettime(CLOCK_REALTIME, &until);
	until.tv_sec += timeoutMs / 1000;
	until.tv_nsec += (long)(timeoutMs % 1000) * 1000000L;
	if (until.tv_nsec >= 1000000000L)
	{
		until.tv_sec++;
		until.tv_nsec -= 1000000000L;
	}

	pthread_mutex_lock(&mutex);
	while (!signaled)
	{
		if (pthread_cond_timedwait(&cond, &mutex, &until) != 0)
		{
			break;		// timed out
		}
	}
	bool was = signaled;
	signaled = false;
	pthread_mutex_unlock(&mutex);
	return was;
#endif
}

#if defined(_WINDOWS)

DWORD WINAPI Thread::RingleaderThreadProc(LPVOID lpParameter)
{
	WrappedContext* wrappedCtx = (WrappedContext*)lpParameter;

	if (wrappedCtx->proc)
	{
		while (true)
		{
			wrappedCtx->proc(wrappedCtx->context);
		}
	}

	return 0;
}

Thread::Thread(ThreadProc threadProc, bool suspended, void* context, const char* name)
{
	running = !suspended;
	threadName = name;

	ctx.context = context;
	ctx.proc = threadProc;
	threadHandle = CreateThread(NULL, StackSize, RingleaderThreadProc, &ctx, suspended ? CREATE_SUSPENDED : 0, &threadId);
	assert(threadHandle != INVALID_HANDLE_VALUE);
}

Thread::~Thread()
{
	Suspend();

	TerminateThread(threadHandle, 0);
	WaitForSingleObject(threadHandle, 1000);
}

void Thread::Resume()
{
	resumeLock.Lock();
	if (!running)
	{
		ResumeThread(threadHandle);
		running = true;
		resumeCounter++;
	}
	resumeLock.Unlock();
}

void Thread::Suspend()
{
	if (running)
	{
		running = false;
		suspendCounter++;
		SuspendThread(threadHandle);
	}
}

void Thread::Sleep(size_t milliseconds)
{
	::Sleep((DWORD)milliseconds);
}

#endif // _WINDOWS


#ifdef _LINUX

// Thanks for the example implementation.
// https://stackoverflow.com/questions/9397068/how-to-pause-a-pthread-any-time-i-want

// Whoever removed suspend / resume from pthread is not a good person.

void* Thread::RingleaderThreadProc(void* args)
{
	Thread* thread = (Thread*)args;

	while (!thread->terminated)
	{
		pthread_mutex_lock(&thread->mutex);

		switch (thread->command)
		{
			// command to pause thread..
			case 0:
				pthread_cond_wait(&thread->cond_var, &thread->mutex);
				break;

			// command to run..
			case 1:
				if (thread->ctx.proc)
				{
					thread->ctx.proc(thread->ctx.context);
				}
				break;
		}

		pthread_mutex_unlock(&thread->mutex);

		// it's important to give main thread few time after unlock 'this'
		pthread_yield();
	}

	thread->terminated = false;

	pthread_exit(nullptr);
}

Thread::Thread(ThreadProc threadProc, bool suspended, void* context, const char* name)
{
	running = !suspended;
	threadName = name;

	ctx.context = context;
	ctx.proc = threadProc;

	pthread_mutex_init(&mutex, nullptr);
	pthread_cond_init(&cond_var, nullptr);

	// create thread in suspended state..
	command = running ? 1 : 0;

	int status = pthread_create(&threadId, nullptr, Thread::RingleaderThreadProc, this);
	assert(status == 0);
}

Thread::~Thread()
{
	terminated = true;

	// Run if suspended
	if (!running)
	{
		Resume();
	}

	// Wait terminated
	while (terminated)
	{
		Thread::Sleep(1);
	}

	pthread_join(threadId, nullptr);

	pthread_cond_destroy(&cond_var);
	pthread_mutex_destroy(&mutex);
}

void Thread::Resume()
{
	resumeLock.Lock();
	if (!running)
	{
		pthread_mutex_lock(&mutex);
		command = 1;
		pthread_cond_signal(&cond_var);
		pthread_mutex_unlock(&mutex);

		running = true;
		resumeCounter++;
	}
	resumeLock.Unlock();
}

void Thread::Suspend()
{
	if (running)
	{
		running = false;
		suspendCounter++;

		pthread_mutex_lock(&mutex);
		command = 0;
		// in pause command we dont need to signal cond_var because we not in wait state now..
		pthread_mutex_unlock(&mutex);
	}
}

void Thread::Sleep(size_t milliseconds)
{
	usleep(milliseconds * 1000);
}

#endif // _LINUX


namespace Util
{
	std::string WstringToString(const std::wstring& wstr)
	{
		std::string str;
		str.reserve(wstr.size());
		for (auto it = wstr.begin(); it != wstr.end(); ++it)
		{
			str.push_back((char)*it);
		}
		return str;
	}

	std::wstring StringToWstring(const std::string& str)
	{
		std::wstring wstr;
		wstr.reserve(str.size());
		for (auto it = str.begin(); it != str.end(); ++it)
		{
			wstr.push_back((wchar_t)*it);
		}
		return wstr;
	}

	size_t FileSize(const std::wstring& filename)
	{
		FILE* f;
#ifdef _LINUX
		f = fopen(Util::WstringToString(filename).c_str(), "rb");
#else
		_wfopen_s(&f, filename.c_str(), L"rb");
#endif
		if (!f)
			return 0;

		fseek(f, 0, SEEK_END);
		size_t size = ftell(f);
		fclose(f);

		return size;
	}

	size_t FileSize(const std::string& filename)
	{
		std::wstring wstr = StringToWstring(filename);
		return FileSize(wstr);
	}

	size_t FileSize(const wchar_t* filename)
	{
		std::wstring wstr(filename);
		return FileSize(wstr);
	}

	bool FileExists(const std::wstring& filename)
	{
		FILE* f;
#ifdef _LINUX
		f = fopen(Util::WstringToString(filename).c_str(), "rb");
#else
		_wfopen_s(&f, filename.c_str(), L"rb");
#endif
		if (!f)
			return false;
		fclose(f);
		return true;
	}

	bool FileExists(const std::string& filename)
	{
		std::wstring wstr = StringToWstring(filename);
		return FileExists(wstr);
	}

	bool FileExists(const wchar_t* filename)
	{
		std::wstring wstr(filename);
		return FileExists(wstr);
	}

	std::vector<uint8_t> FileLoad(const std::wstring& filename)
	{
		if (!FileExists(filename))
		{
			return std::vector<uint8_t>();
		}

		size_t size = FileSize(filename);

		uint8_t* data = new uint8_t[size];

		FILE* f;
#ifdef _LINUX
		f = fopen(Util::WstringToString(filename).c_str(), "rb");
#else
		_wfopen_s(&f, filename.c_str(), L"rb");
#endif

		fread(data, 1, size, f);
		fclose(f);

		std::vector<uint8_t> output(data, data + size);

		delete[] data;

		return output;
	}

	std::vector<uint8_t> FileLoad(const std::string& filename)
	{
		std::wstring wstr = StringToWstring(filename);
		return FileLoad(wstr);
	}

	std::vector<uint8_t> FileLoad(const wchar_t* filename)
	{
		std::wstring wstr(filename);
		return FileLoad(wstr);
	}

	bool FileSave(const std::wstring& filename, std::vector<uint8_t>& data)
	{
		FILE* f;
#ifdef _LINUX
		f = fopen(Util::WstringToString(filename).c_str(), "rb");
#else
		_wfopen_s(&f, filename.c_str(), L"wb");
#endif
		if (!f)
			return false;

		fwrite(data.data(), 1, data.size(), f);
		fclose(f);

		return true;
	}

	bool FileSave(const std::string& filename, std::vector<uint8_t>& data)
	{
		std::wstring wstr = StringToWstring(filename);
		return FileSave(wstr, data);
	}

	bool FileSave(const wchar_t* filename, std::vector<uint8_t>& data)
	{
		std::wstring wstr(filename);
		return FileSave(wstr, data);
	}

	void SplitPath(const char* _Path,
		char* _Drive,
		char* _Dir,
		char* _Filename,
		char* _Ext)
	{

#if defined(_WINDOWS)
		_splitpath(_Path, _Drive, _Dir, _Filename, _Ext);
#endif

#if defined (_LINUX)

		_Drive[0] = 0;

		char filename[0x1000] = { 0, };

		char* base = basename((char*)_Path);

		if (base)
		{
			strcpy(_Filename, base);
			strcpy(_Ext, base);

			char* fnamePtr = strchr(_Filename, '.');
			if (fnamePtr)
			{
				*fnamePtr = 0;
			}
			else
			{
				_Filename[0] = 0;
			}

			char* extPtr = strrchr(_Ext, '.');
			if (extPtr)
			{
				*extPtr = 0;
			}
			else
			{
				_Ext[0] = 0;
			}
		}
		else
		{
			_Filename[0] = 0;
			_Ext[0] = 0;
		}

		char* dir = dirname((char*)_Path);

		if (dir)
		{
			strcpy(_Dir, dir);
		}
		else
		{
			_Dir[0] = 0;
		}


#endif

	}

	/// <summary>
	/// Get a list of files and directories, relative to the root directory.
	/// WARNING! This method is recursive. You must understand what you are doing.
	/// </summary>
	/// <param name="rootDir">Directory relative to which the tree will be built</param>
	/// <param name="names">List of files and directories. The path includes the root directory. 
	/// If the root directory is a full path, then the path in this list to the directory/file will also be full. Otherwise, the paths are relative (but include the root directory).</param>
	void BuildFileTree(std::wstring rootDir, std::list<std::wstring>& names)
	{

		if (rootDir.back() == L'/')
		{
			rootDir.pop_back();
		}

#if defined(_WINDOWS)

		std::wstring search_path = rootDir + L"/*.*";
		WIN32_FIND_DATAW fd = { 0 };
		HANDLE hFind = ::FindFirstFileW(search_path.c_str(), &fd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				std::wstring name = fd.cFileName;

				if (name == L"." || name == L"..")
					continue;

				std::wstring fullPath = rootDir + L"/" + name;

				names.push_back(fullPath);

				if (Util::IsDirectory(fullPath))
				{
					BuildFileTree(fullPath, names);
				}

			} while (::FindNextFileW(hFind, &fd));

			::FindClose(hFind);
		}

#endif

#if defined (_LINUX)

		DIR* dir;
		struct dirent* ent;
		if ((dir = opendir(Util::WstringToString(rootDir).c_str())) != NULL)
		{
			while ((ent = readdir(dir)) != NULL)
			{
				std::wstring name = Util::StringToWstring(ent->d_name);

				if (name == L"." || name == L"..")
					continue;

				std::wstring fullPath = rootDir + L"/" + name;

				names.push_back(fullPath);

				if (Util::IsDirectory(fullPath))
				{
					BuildFileTree(fullPath, names);
				}
			}
			closedir(dir);
		}

#endif


	}

	/// <summary>
	/// Check if the entity is a directory or a file.
	/// </summary>
	/// <param name="path">Path to directory or file (can be relative).</param>
	/// <returns>true: The specified entity is a directory.</returns>
	bool IsDirectory(std::wstring path)
	{

#if defined(_WINDOWS)

		DWORD attr = ::GetFileAttributesW(path.c_str());

		// A path that is not there must not pass for a directory: GetFileAttributes answers
		// INVALID_FILE_ATTRIBUTES, which has every bit set - the directory bit included.

		if (attr == INVALID_FILE_ATTRIBUTES)
		{
			return false;
		}

		return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

#endif

#if defined (_LINUX)

		// Year 2020. Linux still doesn't really know how to work with wchar_t

		struct stat attr;

		// stat() leaves the structure alone when it fails, so the result has to be checked
		// before the mode is looked at.

		if (stat(Util::WstringToString(path).c_str(), &attr) != 0)
		{
			return false;
		}

		return (attr.st_mode & S_IFDIR) != 0;

#endif

		return false;
	}



	// -------------------------------------------------------------------------------------------
	// PNG images
	//
	// A small, dependency-free PNG writer: the pixel data is stored in uncompressed ("stored")
	// deflate blocks, which is a perfectly valid zlib stream, so no compressor is needed. The
	// files are larger than with a real deflate, but they are only used for debug screenshots
	// and for the unit test report.

	namespace
	{
		uint32_t PngCrc32(const uint8_t* data, size_t len)
		{
			static uint32_t table[256];
			static bool tableReady = false;

			if (!tableReady)
			{
				for (uint32_t n = 0; n < 256; n++)
				{
					uint32_t c = n;
					for (int k = 0; k < 8; k++)
					{
						c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
					}
					table[n] = c;
				}
				tableReady = true;
			}

			uint32_t c = 0xffffffffu;
			for (size_t i = 0; i < len; i++)
			{
				c = table[(c ^ data[i]) & 0xff] ^ (c >> 8);
			}
			return c ^ 0xffffffffu;
		}

		uint32_t PngAdler32(const uint8_t* data, size_t len)
		{
			uint32_t a = 1, b = 0;
			for (size_t i = 0; i < len; i++)
			{
				a = (a + data[i]) % 65521;
				b = (b + a) % 65521;
			}
			return (b << 16) | a;
		}

		void PngPut32(std::vector<uint8_t>& out, uint32_t value)
		{
			out.push_back((uint8_t)(value >> 24));
			out.push_back((uint8_t)(value >> 16));
			out.push_back((uint8_t)(value >> 8));
			out.push_back((uint8_t)value);
		}

		void PngChunk(std::vector<uint8_t>& out, const char* type, const uint8_t* data, size_t len)
		{
			PngPut32(out, (uint32_t)len);

			size_t crcStart = out.size();
			out.insert(out.end(), type, type + 4);
			if (len != 0 && data != nullptr)
			{
				out.insert(out.end(), data, data + len);
			}

			PngPut32(out, PngCrc32(out.data() + crcStart, out.size() - crcStart));
		}

		// Wrap the raw image bytes into a zlib stream of stored deflate blocks.
		void PngZlibStored(const std::vector<uint8_t>& raw, std::vector<uint8_t>& out)
		{
			out.push_back(0x78);		// CM = 8 (deflate), CINFO = 7 (32K window)
			out.push_back(0x01);		// FCHECK so that the header is a multiple of 31

			size_t offset = 0;
			do
			{
				size_t blockLen = raw.size() - offset;
				if (blockLen > 65535)
				{
					blockLen = 65535;
				}

				bool last = (offset + blockLen) >= raw.size();

				out.push_back(last ? 1 : 0);
				out.push_back((uint8_t)blockLen);
				out.push_back((uint8_t)(blockLen >> 8));
				out.push_back((uint8_t)~blockLen);
				out.push_back((uint8_t)(~blockLen >> 8));

				out.insert(out.end(), raw.begin() + offset, raw.begin() + offset + blockLen);
				offset += blockLen;

			} while (offset < raw.size());

			PngPut32(out, PngAdler32(raw.data(), raw.size()));
		}
	}

	bool SavePng(const char* filename, const uint8_t* rgb, size_t width, size_t height)
	{
		if (filename == nullptr || rgb == nullptr || width == 0 || height == 0)
		{
			return false;
		}

		// PNG scanlines are prefixed with a filter byte; filter 0 (None) is used here.
		std::vector<uint8_t> raw;
		raw.reserve((width * 3 + 1) * height);

		for (size_t y = 0; y < height; y++)
		{
			raw.push_back(0);
			const uint8_t* row = rgb + y * width * 3;
			raw.insert(raw.end(), row, row + width * 3);
		}

		std::vector<uint8_t> png = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };

		uint8_t ihdr[13];
		ihdr[0] = (uint8_t)(width >> 24); ihdr[1] = (uint8_t)(width >> 16);
		ihdr[2] = (uint8_t)(width >> 8);  ihdr[3] = (uint8_t)width;
		ihdr[4] = (uint8_t)(height >> 24); ihdr[5] = (uint8_t)(height >> 16);
		ihdr[6] = (uint8_t)(height >> 8);  ihdr[7] = (uint8_t)height;
		ihdr[8] = 8;		// bit depth
		ihdr[9] = 2;		// colour type: truecolour
		ihdr[10] = 0;		// compression
		ihdr[11] = 0;		// filter
		ihdr[12] = 0;		// interlace

		PngChunk(png, "IHDR", ihdr, sizeof(ihdr));

		std::vector<uint8_t> zlib;
		PngZlibStored(raw, zlib);
		PngChunk(png, "IDAT", zlib.data(), zlib.size());
		PngChunk(png, "IEND", nullptr, 0);

		FILE* f = fopen(filename, "wb");
		if (f == nullptr)
		{
			return false;
		}

		size_t written = fwrite(png.data(), 1, png.size(), f);
		fclose(f);

		return written == png.size();
	}

#if 0

	void BuildTreeDemo()
	{
		std::list<std::wstring> names;
		Util::BuildFileTree(L"c:/Work/DolphinSDK_Dvddata", names);
		for (auto it = names.begin(); it != names.end(); ++it)
		{
			std::wstring name = *it;

			if (Util::IsDirectory(name))
			{
				Debug::Report(Debug::Channel::Norm, "Dir: %s\n", Util::WstringToString(name).c_str());
			}
			else
			{
				Debug::Report(Debug::Channel::Norm, "File: %s\n", Util::WstringToString(name).c_str());
			}
		}
	}

#endif

}
