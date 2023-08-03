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

#ifdef _WINDOWS

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

        return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

#endif

#if defined (_LINUX)

        // Year 2020. Linux still doesn't really know how to work with wchar_t

        struct stat attr;

        stat(Util::WstringToString(path).c_str(), &attr);

        return (attr.st_mode & S_IFDIR) != 0;

#endif

        return false;
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
