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

		// Deliberately no sched_yield() here. The worker procedure is the hottest path of the whole
		// emulator - the recompiler calls it once per basic block - so a yield costs a system call
		// per block: on Super Mario Sunshine it held the Gekko thread at 3.7 MIPS against 81.9
		// without it (0.12x real time against 2.6x). A thread that has to wait parks on the
		// condition variable below or blocks in the device it drives, and the Windows ringleader
		// (see above) never yielded either.
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

		// The worker procedure runs with the thread's own mutex held (RingleaderThreadProc), so a
		// thread that suspends *itself* would deadlock on the second lock: a pthread mutex is not
		// recursive. Both the AI thread (which parks itself while no DMA is armed) and the DSP one
		// (which parks itself on a breakpoint) do exactly that. Writing the command without the
		// lock is enough: the ringleader parks on the condition variable at the top of its next
		// iteration, and Resume is what signals it.
		if (pthread_equal(pthread_self(), threadId))
		{
			command = 0;
			return;
		}

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
	// -------------------------------------------------------------------------------------------
	// UTF-8
	//
	// The narrow strings of the project are UTF-8 (see the note in utils.h). These are the two
	// conversion directions plus the cursor arithmetic the console command line needs to insert and
	// delete whole characters.

	namespace
	{
		const uint32_t Utf8Replacement = 0xFFFD;

		bool Utf8IsContinuation(uint8_t value)
		{
			return (value & 0xC0) == 0x80;
		}

		// The length of the sequence a leading byte announces, or 0 when the byte cannot start one
		// (a stray continuation byte, or one of the two lengths that would encode an over-long form).
		int Utf8LeadLength(uint8_t lead)
		{
			if (lead < 0x80) return 1;
			if (lead >= 0xC2 && lead <= 0xDF) return 2;
			if (lead >= 0xE0 && lead <= 0xEF) return 3;
			if (lead >= 0xF0 && lead <= 0xF4) return 4;
			return 0;
		}

		// Decode the UTF-8 sequence at `offset`. Returns the code point and its length, or an
		// invalid marker (-1) for a byte that cannot begin a code point here.
		int32_t Utf8Decode(const std::string& str, size_t offset, size_t& length)
		{
			length = 1;

			uint8_t lead = (uint8_t)str[offset];
			int size = Utf8LeadLength(lead);

			if (size == 0)
			{
				return -1;
			}

			if (size == 1)
			{
				return lead;
			}

			if ((offset + size) > str.size())
			{
				return -1;
			}

			int32_t cp = lead & (0x7F >> size);

			for (int n = 1; n < size; n++)
			{
				uint8_t next = (uint8_t)str[offset + n];

				if (!Utf8IsContinuation(next))
				{
					return -1;
				}

				cp = (cp << 6) | (next & 0x3F);
			}

			// The shortest form of a code point, and the surrogate block, which is not a code
			// point at all: a decoder that accepted them would produce text that cannot be encoded
			// back.
			int32_t shortest = (size == 2) ? 0x80 : (size == 3) ? 0x800 : 0x10000;

			if (cp < shortest || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			{
				return -1;
			}

			length = (size_t)size;
			return cp;
		}

		void Utf8Encode(std::string& str, uint32_t cp)
		{
			if (cp < 0x80)
			{
				str.push_back((char)cp);
			}
			else if (cp < 0x800)
			{
				str.push_back((char)(0xC0 | (cp >> 6)));
				str.push_back((char)(0x80 | (cp & 0x3F)));
			}
			else if (cp < 0x10000)
			{
				str.push_back((char)(0xE0 | (cp >> 12)));
				str.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
				str.push_back((char)(0x80 | (cp & 0x3F)));
			}
			else
			{
				str.push_back((char)(0xF0 | (cp >> 18)));
				str.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
				str.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
				str.push_back((char)(0x80 | (cp & 0x3F)));
			}
		}
	}

	std::string WstringToString(const std::wstring& wstr)
	{
		std::string str;
		str.reserve(wstr.size());

		for (size_t i = 0; i < wstr.size(); i++)
		{
			uint32_t cp = (uint32_t)wstr[i];

#if defined(_WINDOWS)

			// A code point outside the BMP is a pair of 16-bit code units here, and the two have to
			// be put together again before they can become four UTF-8 bytes.
			if (cp >= 0xD800 && cp <= 0xDBFF && (i + 1) < wstr.size() &&
				(uint32_t)wstr[i + 1] >= 0xDC00 && (uint32_t)wstr[i + 1] <= 0xDFFF)
			{
				cp = 0x10000 + ((cp - 0xD800) << 10) + ((uint32_t)wstr[i + 1] - 0xDC00);
				i++;
			}

#endif

			// A code unit that is half of a pair on its own is not text; it becomes the
			// replacement character instead of three bytes that no decoder would accept.
			if (cp >= 0xD800 && cp <= 0xDFFF)
			{
				cp = Utf8Replacement;
			}

			Utf8Encode(str, cp);
		}

		return str;
	}

	std::wstring StringToWstring(const std::string& str)
	{
		std::wstring wstr;
		wstr.reserve(str.size());

		size_t offset = 0;

		while (offset < str.size())
		{
			size_t length = 0;
			int32_t cp = Utf8Decode(str, offset, length);

			if (cp < 0)
			{
				// Not a sequence: the byte stands for itself, so nothing is lost on the way.
				wstr.push_back((wchar_t)(uint8_t)str[offset]);
				offset++;
				continue;
			}

			offset += length;

#if defined(_WINDOWS)

			if (cp > 0xFFFF)
			{
				cp -= 0x10000;
				wstr.push_back((wchar_t)(0xD800 + (cp >> 10)));
				wstr.push_back((wchar_t)(0xDC00 + (cp & 0x3FF)));
			}
			else

#endif
			{
				wstr.push_back((wchar_t)cp);
			}
		}

		return wstr;
	}

	size_t Utf8NextOffset(const std::string& str, size_t offset)
	{
		if (offset >= str.size())
		{
			return str.size();
		}

		size_t length = 0;
		size_t remaining = str.size() - offset;

		Utf8Codepoint(str, offset, length);

		// A sequence that is cut short by the end of the string is one code point of its own.
		if (length > remaining)
		{
			length = remaining;
		}

		return offset + length;
	}

	uint32_t Utf8Codepoint(const std::string& str, size_t offset, size_t& length)
	{
		if (offset >= str.size())
		{
			length = 0;
			return 0;
		}

		int32_t cp = Utf8Decode(str, offset, length);

		if (cp < 0)
		{
			// Not a sequence: the byte stands for itself.
			length = 1;
			return (uint8_t)str[offset];
		}

		return (uint32_t)cp;
	}

	size_t Utf8PrevOffset(const std::string& str, size_t offset)
	{
		if (offset == 0)
		{
			return 0;
		}

		if (offset > str.size())
		{
			offset = str.size();
		}

		size_t pos = offset - 1;

		// Walk back over the continuation bytes of the sequence that ends here. A stray
		// continuation byte is a code point of its own, which is what the loop leaves behind.
		size_t limit = (offset >= 4) ? (offset - 4) : 0;

		while (pos > limit && Utf8IsContinuation((uint8_t)str[pos]))
		{
			pos--;
		}

		return pos;
	}

	FILE* FileOpen(const std::wstring& filename, const char* mode)
	{
		FILE* f = nullptr;

#ifdef _LINUX
		f = fopen(WstringToString(filename).c_str(), mode);
#else
		std::wstring wmode(mode, mode + strlen(mode));
		_wfopen_s(&f, filename.c_str(), wmode.c_str());
#endif

		return f;
	}

	FILE* FileOpen(const wchar_t* filename, const char* mode)
	{
		return FileOpen(std::wstring(filename), mode);
	}

	size_t FileSize(const std::wstring& filename)
	{
		FILE* f = FileOpen(filename, "rb");
		if (!f)
			return 0;

		fseek(f, 0, SEEK_END);

		// ftell returns -1 when the position cannot be reported (a pipe, or an error); a caller that
		// sizes a buffer from it would ask for SIZE_MAX bytes.
		long end = ftell(f);
		fclose(f);

		if (end < 0)
			return 0;

		return (size_t)end;
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
		FILE* f = FileOpen(filename, "rb");
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
		FILE* f = FileOpen(filename, "rb");
		if (!f)
		{
			return std::vector<uint8_t>();
		}

		// A size of 0 means either an empty file or a stream whose length cannot be measured
		// (FileSize returns 0 for a failed ftell); neither can be loaded, and a bogus size would
		// size both the allocation and the read.
		size_t size = FileSize(filename);
		if (size == 0)
		{
			fclose(f);
			return std::vector<uint8_t>();
		}

		uint8_t* data = new uint8_t[size];

		// A short read means the file changed under us; the tail of the buffer would be
		// uninitialized, so hand back only what was really read.
		size_t bytesRead = fread(data, 1, size, f);
		fclose(f);

		std::vector<uint8_t> output(data, data + bytesRead);

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
		// "rb" would make every save fail on the first write; the file has to be opened for writing.
		FILE* f = FileOpen(filename, "wb");
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

#if defined (_LINUX)
	// A bounded copy that always terminates and reports a component which does not fit, so an
	// over-long path component fails the split instead of overflowing the caller's buffer.
	static bool SplitPathCopy(char* dst, size_t dstSize, const char* src)
	{
		if (dst == nullptr || dstSize == 0)
			return false;

		if (src == nullptr)
		{
			dst[0] = 0;
			return true;
		}

		size_t len = strlen(src);
		if (len >= dstSize)
		{
			dst[0] = 0;
			return false;
		}

		memcpy(dst, src, len + 1);
		return true;
	}
#endif

	bool SplitPath(const char* _Path,
		char* _Drive, size_t _DriveSize,
		char* _Dir, size_t _DirSize,
		char* _Filename, size_t _FilenameSize,
		char* _Ext, size_t _ExtSize)
	{
		// On failure every component is empty, so a caller that ignores the result cannot build a
		// path out of a half-filled buffer.
		if (_Drive) _Drive[0] = 0;
		if (_Dir) _Dir[0] = 0;
		if (_Filename) _Filename[0] = 0;
		if (_Ext) _Ext[0] = 0;

		if (_Path == nullptr)
			return false;

#if defined(_WINDOWS)
		// _splitpath_s reports a too-long component (ERANGE) instead of copying past the given
		// capacities, which is what the unsized _splitpath did. A UNC prefix is a directory
		// component like any other and has to fit in _DirSize.
		return _splitpath_s(_Path,
			_Drive, _DriveSize,
			_Dir, _DirSize,
			_Filename, _FilenameSize,
			_Ext, _ExtSize) == 0;
#endif

#if defined (_LINUX)

		// The order matters: basename() and dirname() may modify _Path in place, so base has to be
		// copied out before dirname() is called on it.
		char* base = basename((char*)_Path);

		if (!SplitPathCopy(_Filename, _FilenameSize, base) ||
			!SplitPathCopy(_Ext, _ExtSize, base))
		{
			return false;
		}

		if (base)
		{
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

		char* dir = dirname((char*)_Path);

		if (!SplitPathCopy(_Dir, _DirSize, dir))
		{
			return false;
		}

		return true;
#endif

#if !defined(_WINDOWS) && !defined(_LINUX)
		return false;
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

		FILE* f = FileOpen(StringToWstring(filename), "wb");
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
