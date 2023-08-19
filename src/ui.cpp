#include "pch.h"

// About dialog

static bool opened = false;
static HWND dlgAbout;

// dialog procedure
static INT_PTR CALLBACK AboutProc(
	HWND    hwndDlg,    // handle to dialog box
	UINT    uMsg,       // message
	WPARAM  wParam,     // first message parameter
	LPARAM  lParam      // second message parameter
)
{
#ifdef _DEBUG
	auto version = L"Debug";
#else
	auto version = L"Release";
#endif

#if _M_X64
	auto platform = L"x64";
#else
	auto platform = L"x86";
#endif

	auto jitc = UI::Jdi->JitcEnabled() ? L"JITC" : L"";

	UNREFERENCED_PARAMETER(lParam);
	switch (uMsg)
	{
		// prepare swap dialog
		case WM_INITDIALOG:
		{
			dlgAbout = hwndDlg;
			ShowWindow(dlgAbout, SW_NORMAL);
			SendMessage(dlgAbout, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PUREI_ICON)));
			CenterChildWindow(GetParent(dlgAbout), dlgAbout);

			std::string dateStamp = __DATE__;
			std::string timeStamp = __TIME__;

			auto buffer = fmt::format(L"{:s} - {:s}\n{:s}\n{:s} {:s} {:s} {:s} {:s} ({:s} {:s})\n",
				APPNAME, APPDESC,
				L"Copyright 2003-2023 Dolwin team, emu-russia",
				L"Build version",
				Util::StringToWstring(UI::Jdi->GetVersion()),
				version, platform, jitc,
				Util::StringToWstring(dateStamp),
				Util::StringToWstring(timeStamp));

			SetDlgItemText(dlgAbout, IDC_ABOUT_RELEASE, buffer.c_str());
			return true;
		}

		// close button -> kill about
		case WM_CLOSE:
		{
			DestroyWindow(dlgAbout);
			dlgAbout = NULL;
			opened = false;
			break;
		}

		case WM_COMMAND:
		{
			if (wParam == IDCANCEL)
			{
				DestroyWindow(dlgAbout);
				dlgAbout = NULL;
				opened = false;
				return TRUE;
			}

			if (wParam == IDOK)
			{
				DestroyWindow(dlgAbout);
				dlgAbout = NULL;
				opened = false;
				return TRUE;
			}

			break;
		}
	}

	return FALSE;
}

// non-blocking call
void AboutDialog(HWND hwndParent)
{
	if (opened) return;

	// create modeless dialog
	CreateDialog(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDD_ABOUT),
		hwndParent,
		AboutProc
	);

	opened = true;
}

Json::Value* CmdUIError(std::vector<std::string>& args)
{
	std::string text = "";

	if (args.size() < 2)
	{
		return nullptr;
	}

	for (size_t i = 1; i < args.size(); i++)
	{
		text += args[i] + " ";
	}

	UI::Error(L"Error", L"%s", Util::StringToWstring(text).c_str());

	return nullptr;
}

Json::Value* CmdUIReport(std::vector<std::string>& args)
{
	std::string text = "";

	if (args.size() < 2)
	{
		return nullptr;
	}

	for (size_t i = 1; i < args.size(); i++)
	{
		text += args[i] + " ";
	}

	UI::Report(L"%s", Util::StringToWstring(text).c_str());

	return nullptr;
}

Json::Value* CmdGetRenderTarget(std::vector<std::string>& args)
{
	// Return HWND as RenderTarget

	Json::Value* value = new Json::Value();
	value->type = Json::ValueType::Int;
	value->value.AsInt = (uint64_t)wnd.hMainWindow;
	return value;
}

void UIReflector()
{
	JdiAddCmd("UIError", CmdUIError);
	JdiAddCmd("UIReport", CmdUIReport);
	JdiAddCmd("GetRenderTarget", CmdGetRenderTarget);
}

// UI file utilities

namespace UI
{
	// Open file/directory dialog
	const wchar_t* FileOpenDialog(FileType type)
	{
		HWND hwnd = wnd.hMainWindow;
		static wchar_t tempBuf[0x1000] = { 0 };
		OPENFILENAME ofn;
		wchar_t szFileName[1024];
		wchar_t szFileTitle[1024];
		wchar_t prevDir[1024];
		std::string lastDir;
		BOOL result;

		GetCurrentDirectory(sizeof(prevDir), prevDir);

		switch (type)
		{
			case FileType::All:
			case FileType::Json:
				lastDir = UI::Jdi->GetConfigString(USER_LASTDIR_ALL, USER_UI);
				break;
			case FileType::Dvd:
				lastDir = UI::Jdi->GetConfigString(USER_LASTDIR_DVD, USER_UI);
				break;
			case FileType::Map:
				lastDir = UI::Jdi->GetConfigString(USER_LASTDIR_MAP, USER_UI);
				break;
		}

		memset(szFileName, 0, sizeof(szFileName));
		memset(szFileTitle, 0, sizeof(szFileTitle));

		if (type == FileType::Directory)
		{
			BROWSEINFO bi;
			LPTSTR lpBuffer = NULL;
			LPITEMIDLIST pidlRoot = NULL;     // PIDL for root folder 
			LPITEMIDLIST pidlBrowse = NULL;   // PIDL selected by user
			LPMALLOC g_pMalloc;

			// Get the shell's allocator. 
			if (!SUCCEEDED(SHGetMalloc(&g_pMalloc)))
				return L"";

			// Allocate a buffer to receive browse information.
			lpBuffer = (LPTSTR)g_pMalloc->Alloc(MAX_PATH);
			if (lpBuffer == NULL) return L"";

			// Get the PIDL for the root folder.
			if (!SUCCEEDED(SHGetSpecialFolderLocation(hwnd, CSIDL_DRIVES, &pidlRoot)))
			{
				g_pMalloc->Free(lpBuffer);
				return L"";
			}

			// Fill in the BROWSEINFO structure. 
			bi.hwndOwner = hwnd;
			bi.pidlRoot = pidlRoot;
			bi.pszDisplayName = lpBuffer;
			bi.lpszTitle = L"Choose Directory";
			bi.ulFlags = 0;
			bi.lpfn = NULL;
			bi.lParam = 0;

			// Browse for a folder and return its PIDL. 
			pidlBrowse = SHBrowseForFolder(&bi);
			result = (pidlBrowse != NULL);
			if (result)
			{
				SHGetPathFromIDList(pidlBrowse, lpBuffer);
				wcscpy_s(szFileName, _countof(szFileName) - 1, lpBuffer);

				// Free the PIDL returned by SHBrowseForFolder.
				g_pMalloc->Free(pidlBrowse);
			}

			// Clean up. 
			if (pidlRoot) g_pMalloc->Free(pidlRoot);
			if (lpBuffer) g_pMalloc->Free(lpBuffer);

			// Release the shell's allocator. 
			g_pMalloc->Release();
		}
		else
		{
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hwnd;
			switch (type)
			{
				case FileType::All:
					ofn.lpstrFilter =
						L"All Supported Files (*.dol, *.elf, *.gcm, *.iso)\0*.dol;*.elf;*.gcm;*.iso\0"
						L"GameCube Executable Files (*.dol, *.elf)\0*.dol;*.elf\0"
						L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
						L"All Files (*.*)\0*.*\0";
					break;
				case FileType::Dvd:
					ofn.lpstrFilter =
						L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
						L"All Files (*.*)\0*.*\0";
					break;
				case FileType::Map:
					ofn.lpstrFilter =
						L"Symbolic information files (*.map)\0*.map\0"
						L"All Files (*.*)\0*.*\0";
					break;
				case FileType::Json:
					ofn.lpstrFilter =
						L"Json files (*.json)\0*.json\0"
						L"All Files (*.*)\0*.*\0";
					break;
			}

			ofn.lpstrCustomFilter = NULL;
			ofn.nMaxCustFilter = 0;
			ofn.nFilterIndex = 1;
			ofn.lpstrFile = szFileName;
			ofn.nMaxFile = sizeof(szFileName);
			ofn.lpstrInitialDir = Util::StringToWstring(lastDir).c_str();
			ofn.lpstrFileTitle = szFileTitle;
			ofn.nMaxFileTitle = sizeof(szFileTitle);
			ofn.lpstrTitle = L"Open File\0";
			ofn.lpstrDefExt = L"";
			ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

			result = GetOpenFileName(&ofn);
		}

		if (result)
		{
			wcscpy_s(tempBuf, _countof(tempBuf) - 1, szFileName);

			// save last directory

			lastDir = Util::WstringToString(tempBuf);

			while (lastDir.back() != '\\')
			{
				lastDir.pop_back();
			}

			switch (type)
			{
				case FileType::All:
				case FileType::Json:
					UI::Jdi->SetConfigString(USER_LASTDIR_ALL, lastDir, USER_UI);
					break;
				case FileType::Dvd:
					UI::Jdi->SetConfigString(USER_LASTDIR_DVD, lastDir, USER_UI);
					break;
				case FileType::Map:
					UI::Jdi->SetConfigString(USER_LASTDIR_MAP, lastDir, USER_UI);
					break;
			}

			SetCurrentDirectory(prevDir);
			return tempBuf;
		}
		else
		{
			SetCurrentDirectory(prevDir);
			return L"";
		}
	}

	// Save file dialog
	const wchar_t* FileSaveDialog(FileType type)
	{
		HWND hwnd = wnd.hMainWindow;
		static wchar_t tempBuf[0x1000] = { 0 };
		OPENFILENAME ofn;
		wchar_t szFileName[1024];
		wchar_t szFileTitle[1024];
		wchar_t prevDir[1024];
		std::string lastDir;
		BOOL result;

		GetCurrentDirectory(sizeof(prevDir), prevDir);

		switch (type)
		{
			case FileType::All:
			case FileType::Json:
				lastDir = UI::Jdi->GetConfigString(USER_LASTDIR_ALL, USER_UI);
				break;
			case FileType::Dvd:
				lastDir = UI::Jdi->GetConfigString(USER_LASTDIR_DVD, USER_UI);
				break;
			case FileType::Map:
				lastDir = UI::Jdi->GetConfigString(USER_LASTDIR_MAP, USER_UI);
				break;
		}

		memset(szFileName, 0, sizeof(szFileName));
		memset(szFileTitle, 0, sizeof(szFileTitle));

		{
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hwnd;
			switch (type)
			{
				case FileType::All:
					ofn.lpstrFilter =
						L"All Supported Files (*.dol, *.elf, *.gcm, *.iso)\0*.dol;*.elf;*.gcm;*.iso\0"
						L"GameCube Executable Files (*.dol, *.elf)\0*.dol;*.elf\0"
						L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
						L"All Files (*.*)\0*.*\0";
					break;
				case FileType::Dvd:
					ofn.lpstrFilter =
						L"GameCube DVD Images (*.gcm, *.iso)\0*.gcm;*.iso\0"
						L"All Files (*.*)\0*.*\0";
					break;
				case FileType::Map:
					ofn.lpstrFilter =
						L"Symbolic information files (*.map)\0*.map\0"
						L"All Files (*.*)\0*.*\0";
					break;
				case FileType::Json:
					ofn.lpstrFilter =
						L"Json files (*.json)\0*.json\0"
						L"All Files (*.*)\0*.*\0";
					break;
			}

			ofn.lpstrCustomFilter = NULL;
			ofn.nMaxCustFilter = 0;
			ofn.nFilterIndex = 1;
			ofn.lpstrFile = szFileName;
			ofn.nMaxFile = sizeof(szFileName);
			ofn.lpstrInitialDir = Util::StringToWstring(lastDir).c_str();
			ofn.lpstrFileTitle = szFileTitle;
			ofn.nMaxFileTitle = sizeof(szFileTitle);
			ofn.lpstrTitle = L"Save File\0";
			ofn.lpstrDefExt = L"";
			ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

			result = GetSaveFileName(&ofn);
		}

		if (result)
		{
			wcscpy_s(tempBuf, _countof(tempBuf) - 1, szFileName);

			// save last directory

			lastDir = Util::WstringToString(tempBuf);

			while (lastDir.back() != '\\')
			{
				lastDir.pop_back();
			}

			switch (type)
			{
				case FileType::All:
				case FileType::Json:
					UI::Jdi->SetConfigString(USER_LASTDIR_ALL, lastDir, USER_UI);
					break;
				case FileType::Dvd:
					UI::Jdi->SetConfigString(USER_LASTDIR_DVD, lastDir, USER_UI);
					break;
				case FileType::Map:
					UI::Jdi->SetConfigString(USER_LASTDIR_MAP, lastDir, USER_UI);
					break;
			}

			SetCurrentDirectory(prevDir);
			return tempBuf;
		}
		else
		{
			SetCurrentDirectory(prevDir);
			return L"";
		}
	}

	// make path to file shorter for "lvl" levels.
	wchar_t* FileShortName(const wchar_t* filename, int lvl)
	{
		static wchar_t tempBuf[1024] = { 0 };

		int c = 0;
		size_t i = 0;

		wchar_t* ptr = (wchar_t*)filename;

		tempBuf[0] = ptr[0];
		tempBuf[1] = ptr[1];
		tempBuf[2] = ptr[2];

		ptr += 3;

		for (i = wcslen(ptr) - 1; i; i--)
		{
			if (ptr[i] == L'\\') c++;
			if (c == lvl) break;
		}

		if (c == lvl)
		{
			swprintf_s(&tempBuf[3], _countof(tempBuf) - 3, L"...%s", &ptr[i]);
		}
		else return ptr - 3;

		return tempBuf;
	}

	/* Make path to file shorter for "lvl" levels. */
	std::wstring FileShortName(const std::wstring& filename, int lvl)
	{
		static wchar_t tempBuf[1024] = { 0 };

		int c = 0;
		size_t i = 0;

		wchar_t* ptr = (wchar_t*)filename.data();

		tempBuf[0] = ptr[0];
		tempBuf[1] = ptr[1];
		tempBuf[2] = ptr[2];

		ptr += 3;

		for (i = wcslen(ptr) - 1; i; i--)
		{
			if (ptr[i] == L'\\') c++;
			if (c == lvl) break;
		}

		if (c == lvl)
		{
			swprintf_s(&tempBuf[3], _countof(tempBuf) - 3, L"...%s", &ptr[i]);
		}
		else return ptr - 3;

		return tempBuf;
	}

	/* Nice value of KB, MB or GB, for output. */
	std::wstring FileSmartSize(size_t size)
	{
		static auto tempBuf = std::wstring();

		if (size < 1024)
		{
			tempBuf = fmt::sprintf(L"%zi byte", size);
		}
		else if (size < 1024 * 1024)
		{
			tempBuf = fmt::sprintf(L"%zi KB", size / 1024);
		}
		else if (size < 1024 * 1024 * 1024)
		{
			tempBuf = fmt::sprintf(L"%zi MB", size / 1024 / 1024);
		}
		else
		{
			tempBuf = fmt::sprintf(L"%1.1f GB", (float)size / 1024 / 1024 / 1024);
		}

		return tempBuf;
	}

	std::string FileSmartSizeA(size_t size)
	{
		static auto tempBuf = std::string(1024, 0);

		if (size < 1024)
		{
			tempBuf = fmt::format("%zi byte", size);
		}
		else if (size < 1024 * 1024)
		{
			tempBuf = fmt::format("%zi KB", size / 1024);
		}
		else if (size < 1024 * 1024 * 1024)
		{
			tempBuf = fmt::format("%zi MB", size / 1024 / 1024);
		}
		else
		{
			tempBuf = fmt::format("%1.1f GB", (float)size / 1024 / 1024 / 1024);
		}

		return tempBuf;
	}
}



// Basic application output

namespace UI
{
	// fatal error
	void Error(const wchar_t* title, const wchar_t* fmt, ...)
	{
		va_list arg;
		wchar_t buf[0x1000];

		va_start(arg, fmt);
		vswprintf_s(buf, _countof(buf) - 1, fmt, arg);
		va_end(arg);

		MessageBox(NULL, buf, title, MB_ICONHAND | MB_OK | MB_TOPMOST);
		if (IsWindow(wnd.hMainWindow))
		{
			SendMessage(wnd.hMainWindow, WM_CLOSE, 0, 0);
		}
		else
		{
			exit(-1);
		}
	}

	// application message
	void Report(const wchar_t* fmt, ...)
	{
		va_list arg;
		wchar_t buf[0x1000];

		va_start(arg, fmt);
		vswprintf_s(buf, _countof(buf) - 1, fmt, arg);
		va_end(arg);

		MessageBox(NULL, buf, APPNAME L" Reports", MB_ICONINFORMATION | MB_OK | MB_TOPMOST);
	}
}

// WinMain (very first run-time initialization and main loop)

// Check for multiple instancies.
static void LockMultipleCalls()
{
	static HANDLE sema;

	// mutex will fail if semephore already exists
	sema = CreateMutex(NULL, 0, APPNAME);
	if (sema == NULL)
	{
		auto app_name = std::wstring(APPNAME);
		UI::Report(L"We are already running %s!!", app_name);
		exit(0);    // return good
	}
	CloseHandle(sema);

	sema = CreateSemaphore(NULL, 0, 1, APPNAME);
}

static bool IsDirectoryExists(LPCWSTR szPath)
{
	DWORD dwAttrib = GetFileAttributesW(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// return file name without quotes
char* FixCommandLine(char* lpCmdLine)
{
	if (*lpCmdLine == '\"' || *lpCmdLine == '\'')
	{
		lpCmdLine++;
	}
	size_t len = strlen(lpCmdLine);
	if (lpCmdLine[len - 1] == '\"' || lpCmdLine[len - 1] == '\'')
	{
		lpCmdLine[len - 1] = 0;
	}
	return lpCmdLine;
}

/* Keyboard accelerators (no need to be shared). */
HACCEL  hAccel;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nShowCmd);

	EMUCtor();

	// Create an interface for communicating with the emulator core

	UI::Jdi = new UI::JdiClient;

	// Allow only one instance of application to run at once?
	if (UI::Jdi->GetConfigBool(USER_RUNONCE, USER_UI))
	{
		LockMultipleCalls();
	}

	hAccel = LoadAccelerators(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_ACCELERATOR));

	// Start the emulator and user interface
	CreateMainWindow(hInstance);

	// Main loop
	MSG msg = { 0 };
	while (true)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) == 0)
		{
			Sleep(1);
		}

		/* Idle loop */
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (!TranslateAccelerator(wnd.hMainWindow, hAccel, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	// Should never reach this point. Emu always exits.
	UI::Error(L"Error", L"SHOULD NEVER REACH HERE");
	return -2;
}



int um_num;
BOOL um_filechanged;
wchar_t Memcard_filename[2][0x1000];

static wchar_t* NewMemcardFileProc(HWND hwnd, wchar_t* lastDir)
{
	wchar_t prevc[MAX_PATH];
	OPENFILENAME ofn;
	wchar_t szFileName[120];
	wchar_t szFileTitle[120];
	static wchar_t LoadedFile[MAX_PATH];

	GetCurrentDirectory(sizeof(prevc), prevc);

	memset(&szFileName, 0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter =
		L"GameCube Memcard Files (*.mci)\0*.mci\0"
		L"All Files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = 120;
	ofn.lpstrInitialDir = lastDir;
	ofn.lpstrFileTitle = szFileTitle;
	ofn.nMaxFileTitle = 120;
	ofn.lpstrTitle = L"Create Memcard File\0";
	ofn.lpstrDefExt = L"";
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

	if (GetSaveFileName(&ofn))
	{
		int i;

		for (i = 0; i < 120; i++)
		{
			LoadedFile[i] = szFileName[i];
		}

		LoadedFile[i] = '\0';       // terminate

		SetCurrentDirectory(prevc);
		return LoadedFile;
	}
	else
	{
		SetCurrentDirectory(prevc);
		return NULL;
	}
}

static wchar_t* ChooseMemcardFileProc(HWND hwnd, wchar_t* lastDir)
{
	wchar_t prevc[MAX_PATH];
	OPENFILENAME ofn;
	wchar_t szFileName[120];
	wchar_t szFileTitle[120];
	static wchar_t LoadedFile[MAX_PATH];

	GetCurrentDirectory(sizeof(prevc), prevc);

	memset(&szFileName, 0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter =
		L"GameCube Memcard Files (*.mci)\0*.mci\0"
		L"All Files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = 120;
	ofn.lpstrInitialDir = lastDir;
	ofn.lpstrFileTitle = szFileTitle;
	ofn.nMaxFileTitle = 120;
	ofn.lpstrTitle = L"Open Memcard File\0";
	ofn.lpstrDefExt = L"";
	ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (GetOpenFileName(&ofn))
	{
		int i;

		for (i = 0; i < 120; i++)
		{
			LoadedFile[i] = szFileName[i];
		}

		LoadedFile[i] = '\0';       // terminate

		SetCurrentDirectory(prevc);
		return LoadedFile;
	}
	else
	{
		SetCurrentDirectory(prevc);
		return NULL;
	}
}

/*
 * Callback procedure for the choose size (of a new memcard) dialog
 */
static INT_PTR CALLBACK MemcardChooseSizeProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	int index;
	wchar_t buf[256] = { 0 };

	switch (uMsg)
	{
		case WM_INITDIALOG:
			CenterChildWindow(wnd.hMainWindow, hwndDlg);
			SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PUREI_ICON)));

			for (index = 0; index < Num_Memcard_ValidSizes; index++) {
				int blocks, kb;
				blocks = Memcard_ValidSizes[index] / Memcard_BlockSize;
				kb = Memcard_ValidSizes[index] / 1024;
				swprintf_s(buf, _countof(buf) - 1, L"%d blocks  (%d Kb)", blocks, kb);
				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZES, CB_INSERTSTRING, (WPARAM)index, (LPARAM)buf);
			}
			SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZES, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

			return TRUE;
		case WM_CLOSE:
			EndDialog(hwndDlg, -1);
			return TRUE;
		case WM_COMMAND:
			switch (wParam) {
			case IDCANCEL:
				EndDialog(hwndDlg, -1);
				return TRUE;
			case IDOK:
				index = (int)SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZES, CB_GETCURSEL, 0, 0);
				EndDialog(hwndDlg, index);

				return TRUE;
			}
			return FALSE;
		default:
			return FALSE;
	}
	return FALSE;
}

/*
 * Callback procedure for the memcard settings dialog
 */
static INT_PTR CALLBACK MemcardSettingsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	wchar_t buf[MAX_PATH] = { 0 }, buf2[MAX_PATH] = { 0 }, * filename;
	size_t newsize;
	size_t fileSize;

	switch (uMsg)
	{
		case WM_INITDIALOG:
			CenterChildWindow(wnd.hMainWindow, hwndDlg);
			SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PUREI_ICON)));

			if (um_num == 0)
				SendMessage(hwndDlg, WM_SETTEXT, (WPARAM)0, lParam = (LPARAM)L"Memcard A Settings");
			else if (um_num == 1)
				SendMessage(hwndDlg, WM_SETTEXT, (WPARAM)0, lParam = (LPARAM)L"Memcard B Settings");

			SyncSave = UI::Jdi->GetConfigBool(Memcard_SyncSave_Key, USER_MEMCARDS);

			if (um_num == 0)
			{
				Memcard_Connected[um_num] = UI::Jdi->GetConfigBool(MemcardA_Connected_Key, USER_MEMCARDS);
				wcscpy_s(Memcard_filename[um_num], _countof(Memcard_filename[um_num]), Util::StringToWstring(UI::Jdi->GetConfigString(MemcardA_Filename_Key, USER_MEMCARDS)).c_str());
			}
			else if (um_num == 1)
			{
				Memcard_Connected[um_num] = UI::Jdi->GetConfigBool(MemcardB_Connected_Key, USER_MEMCARDS);
				wcscpy_s(Memcard_filename[um_num], _countof(Memcard_filename[um_num]), Util::StringToWstring(UI::Jdi->GetConfigString(MemcardB_Filename_Key, USER_MEMCARDS)).c_str());
			}

			if (SyncSave)
				CheckRadioButton(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE,
					IDC_MEMCARD_SYNCSAVE_TRUE, IDC_MEMCARD_SYNCSAVE_TRUE);
			else
				CheckRadioButton(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE,
					IDC_MEMCARD_SYNCSAVE_TRUE, IDC_MEMCARD_SYNCSAVE_FALSE);

			if (Memcard_Connected[um_num])
				CheckDlgButton(hwndDlg, IDC_MEMCARD_CONNECTED, BST_CHECKED);

			wcscpy_s(buf, _countof(buf) - 1, Memcard_filename[um_num]);
			filename = wcsrchr(buf, L'\\');
			if (filename == NULL) {
				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
			}
			else {
				*filename = L'\0';
				filename++;
				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)filename);
				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
			}

			if (Util::FileExists(buf))
			{
				fileSize = Util::FileSize(buf);
			}
			else
			{
				Memcard_Connected[um_num] = false;
				fileSize = 0;
			}

			if (Memcard_Connected[um_num]) {
				swprintf_s(buf, _countof(buf) - 1, L"Size: %d usable blocks (%d Kb)",
					(int)(fileSize / Memcard_BlockSize - 5), (int)(fileSize / 1024));
				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
			}
			else {
				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT, (WPARAM)0, (LPARAM)L"Not connected");
			}

			um_filechanged = FALSE;
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
			return TRUE;
		case WM_COMMAND:
			switch (wParam) {
			case IDC_MEMCARD_NEW:
				newsize = DialogBox(GetModuleHandle(NULL),
					MAKEINTRESOURCE(IDD_MEMCARD_CHOOSESIZE),
					hwndDlg,
					MemcardChooseSizeProc);
				if (newsize == -1) return TRUE;
				newsize = Memcard_ValidSizes[newsize];

				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT, (WPARAM)256, (LPARAM)(LPCTSTR)buf);
				filename = NewMemcardFileProc(hwndDlg, buf);
				if (filename == NULL) return TRUE;
				wcscpy_s(buf, _countof(buf) - 1, filename);

				/* create the file */
				if (MCCreateMemcardFile(filename, (uint16_t)(newsize >> 17)) == FALSE) return TRUE;

				filename = wcsrchr(buf, L'\\');
				if (filename == NULL) {
					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
				}
				else {
					*filename = '\0';
					filename++;
					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)filename);
					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
				}

				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT, (WPARAM)0, (LPARAM)L"Not connected");

				um_filechanged = TRUE;
				return TRUE;

			case IDC_MEMCARD_CHOOSEFILE:
				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT, (WPARAM)256, (LPARAM)(LPCTSTR)buf);
				filename = ChooseMemcardFileProc(hwndDlg, buf);
				if (filename == NULL) return TRUE;
				wcscpy_s(buf, _countof(buf) - 1, filename);

				filename = wcsrchr(buf, L'\\');
				if (filename == NULL) {
					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
				}
				else {
					*filename = '\0';
					filename++;
					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)filename);
					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
				}
				SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT, (WPARAM)0, (LPARAM)L"Not connected");

				um_filechanged = TRUE;
				return TRUE;
			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				return TRUE;
			case IDOK:
				if (um_filechanged == TRUE)
				{
					size_t Fnsize, Pathsize;
					Fnsize = SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0);
					Pathsize = SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0);

					if (Fnsize + 1 + Pathsize + 1 >= sizeof(Memcard_filename[um_num])) {
						swprintf_s(buf, _countof(buf) - 1, L"File full path must be less than %zi characters.", sizeof(Memcard_filename[um_num]));
						MessageBox(hwndDlg, buf, L"Invalid filename", 0);
						return TRUE;
					}

					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT, (WPARAM)(Pathsize + 1), (LPARAM)(LPCTSTR)buf);
					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_GETTEXT, (WPARAM)(Fnsize + 1), (LPARAM)(LPCTSTR)buf2);

					wcscat_s(buf, _countof(buf) - 1, L"\\");
					wcscat_s(buf, _countof(buf) - 1, buf2);
				}
				else
				{
					size_t Fnsize, Pathsize;
					Fnsize = SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0);
					Pathsize = SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0);

					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT, (WPARAM)(Pathsize + 1), (LPARAM)(LPCTSTR)buf);
					SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_GETTEXT, (WPARAM)(Fnsize + 1), (LPARAM)(LPCTSTR)buf2);

					wcscat_s(buf, _countof(buf) - 1, L"\\");
					wcscat_s(buf, _countof(buf) - 1, buf2);
				}

				if (IsDlgButtonChecked(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE) == BST_CHECKED)
					SyncSave = false;
				else
					SyncSave = true;

				if (IsDlgButtonChecked(hwndDlg, IDC_MEMCARD_CONNECTED) == BST_CHECKED) {
					/* memcard is supposed to be connected */

					Memcard_Connected[um_num] = true;
				}
				else {
					/* memcard is supposed to be disconnected */

					Memcard_Connected[um_num] = false;
				}

				// Writeback settings

				if (um_num == 0)
				{
					wcscpy_s(Memcard_filename[0], _countof(Memcard_filename[0]), buf);
					UI::Jdi->SetConfigBool(MemcardA_Connected_Key, Memcard_Connected[0], USER_MEMCARDS);
					UI::Jdi->SetConfigString(MemcardA_Filename_Key, Util::WstringToString(Memcard_filename[0]), USER_MEMCARDS);
				}
				else
				{
					wcscpy_s(Memcard_filename[1], _countof(Memcard_filename[1]), buf);
					UI::Jdi->SetConfigBool(MemcardB_Connected_Key, Memcard_Connected[1], USER_MEMCARDS);
					UI::Jdi->SetConfigString(MemcardB_Filename_Key, Util::WstringToString(Memcard_filename[1]), USER_MEMCARDS);
				}
				UI::Jdi->SetConfigBool(Memcard_SyncSave_Key, SyncSave, USER_MEMCARDS);

				EndDialog(hwndDlg, 0);
				return TRUE;
			}
			return FALSE;
		default:
			return FALSE;
	}
	return FALSE;
}

/*
 * Calls the memcard settings dialog
 */
void MemcardConfigure(int num, HWND hParent) {
	if ((num != 0) && (num != 1)) return;
	um_num = num;

	DialogBox(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDD_MEMCARD_SETTINGS),
		hParent,
		MemcardSettingsProc);
}



// PAD configure dialog

typedef struct
{
	int         padToConfigure;
	PADCONF     config[4];
} PAD;

PAD pad;

static const wchar_t* vkeys[256] = { // default keyboard virtual codes description (? - not used)
 L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"Bkspace", L"Tab", L"?", L"?", L"?", L"Enter", L"?", L"?", // 00-0F
 L"Shift", L"Control", L"Alt", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", // 10-1F
 L"Space", L"PgUp", L"PgDown", L"End", L"Home", L"Left", L"Up", L"Right", L"Down", L"?", L"?", L"?", L"?", L"Ins", L"Del", L"?", // 20-2F
 L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"?", L"?", L"?", L"?", L"?", L"?", // 30-3F
 L"", L"A", L"B", L"C", L"D", L"E", L"F", L"G", L"H",  L"I", L"J", L"K", L"L", L"M", L"N", L"O", // 40-4F
 L"P", L"Q", L"R", L"S", L"T", L"U", L"V", L"W", L"X", L"Y", L"Z", L"?", L"?", L"?", L"?", L"?", // 50-5F
 L"Num 0", L"Num 1", L"Num 2", L"Num 3", L"Num 4", L"Num 5", L"Num 6", L"Num 7",
 L"Num 8", L"Num 9", L"Mult", L"Plus", L"Bkslash", L"Minus", L"Decimal", L"Slash", // 60-6F
 L"F1", L"F2", L"F3", L"F4", L"F5", L"F6", L"F7", L"F8", L"F9", L"F10", L"F11", L"F12", L"?", L"?", L"?", L"?", // 70-7F
};

static const wchar_t* GetVKDesc(int vkey)
{
	if (vkey >= 0x80) return L"?";
	else return vkeys[vkey];
}

void PADLoadConfig(HWND hwndDlg)
{
	char parm[256] = { 0 };
	int vkey;

	// Plugged or not
	sprintf_s(parm, sizeof(parm), "PluggedIn""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].plugged = UI::Jdi->GetConfigBool(parm, USER_PADS);

	//
	// Buttons 8|
	//

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_UP""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_DOWN""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_LEFT""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_RIGHT""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT] = UI::Jdi->GetConfigInt(parm, USER_PADS);

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP50""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP100""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN50""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN100""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT50""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT100""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT50""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT100""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = UI::Jdi->GetConfigInt(parm, USER_PADS);

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXUP""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXDOWN""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXLEFT""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXRIGHT""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = UI::Jdi->GetConfigInt(parm, USER_PADS);

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERL""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERR""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERZ""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = UI::Jdi->GetConfigInt(parm, USER_PADS);

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_A""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_B""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_X""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_Y""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_START""_%i", pad.padToConfigure);
	pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START] = UI::Jdi->GetConfigInt(parm, USER_PADS);

	//
	// Enable buttons
	//

	if (hwndDlg)
	{
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_UP), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWN), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_LEFT), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RIGHT), TRUE);

		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP50), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP100), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN50), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN100), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT50), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT100), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT50), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT100), TRUE);

		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXUP), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXDOWN), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXLEFT), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXRIGHT), TRUE);

		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERL), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERR), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERZ), TRUE);

		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_A), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_B), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_X), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_Y), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_START), TRUE);
	}

	//
	// Set buttons
	//

	if (hwndDlg)
	{
		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_UP, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_UP, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_A, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_A, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_B, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_B, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_X, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_X, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_Y, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_Y, GetVKDesc(vkey));

		vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START];
		if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_START, L"...");
		else SetDlgItemText(hwndDlg, IDC_BUTTON_START, GetVKDesc(vkey));
	}

	if (!pad.config[pad.padToConfigure].plugged)
	{
		CheckDlgButton(hwndDlg, IDC_CHECK_PLUG, BST_UNCHECKED);

		//
		// Disable buttons
		//

		if (hwndDlg)
		{
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_UP), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWN), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_LEFT), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RIGHT), FALSE);

			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP50), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP100), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN50), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN100), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT50), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT100), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT50), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT100), FALSE);

			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXUP), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXDOWN), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXLEFT), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXRIGHT), FALSE);

			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERL), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERR), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERZ), FALSE);

			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_A), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_B), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_X), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_Y), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_START), FALSE);
		}

		return;
	}
	else CheckDlgButton(hwndDlg, IDC_CHECK_PLUG, BST_CHECKED);
}

void PADSaveConfig(HWND hwndDlg)
{
	char parm[256];

	// Plugged or not
	sprintf_s(parm, sizeof(parm), "PluggedIn""_%i", pad.padToConfigure);
	if (pad.config[pad.padToConfigure].plugged) UI::Jdi->SetConfigBool(parm, true, USER_PADS);
	else UI::Jdi->SetConfigBool(parm, false, USER_PADS);

	//
	// Buttons
	//

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_UP""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_DOWN""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_LEFT""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_RIGHT""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT], USER_PADS);

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP50""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP100""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN50""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN100""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT50""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT100""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT50""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT100""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100], USER_PADS);

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXUP""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXDOWN""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXLEFT""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXRIGHT""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT], USER_PADS);

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERL""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERR""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERZ""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ], USER_PADS);

	sprintf_s(parm, sizeof(parm), "VKEY_FOR_A""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_B""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_X""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_Y""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y], USER_PADS);
	sprintf_s(parm, sizeof(parm), "VKEY_FOR_START""_%i", pad.padToConfigure);
	UI::Jdi->SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START], USER_PADS);
}

// reset all butons and unplug pad
void PADClearConfig(HWND hwndDlg)
{
	int i;

	pad.config[pad.padToConfigure].plugged = false;

	for (i = 0; i < VKEY_FOR_MAX; i++)
	{
		pad.config[pad.padToConfigure].vkeys[i] = 0;
	}

	// reload
	PADSaveConfig(hwndDlg);
	PADLoadConfig(hwndDlg);
}

// set default buttons (only first pad supported)
void PADDefaultConfig(HWND hwndDlg)
{
	switch (pad.padToConfigure)
	{
		case 0:
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP] = 0x24;        // Home
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN] = 0x23;      // End
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT] = 0x2e;      // Del
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT] = 0x22;     // PgDn
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = 0;
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = 0x26;    // Up
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = 0;
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = 0x28;  // Down
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = 0;
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = 0x25;  // Left
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = 0;
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = 0x27; // Right
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP] = 0x68;      // Num 8
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN] = 0x62;    // Num 2
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT] = 0x64;    // Num 4
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = 0x66;   // Num 6
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = 0x51;  // Q
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = 0x57;  // W
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = 0x45;  // E
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A] = 0x58;         // X
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B] = 0x5a;         // Z
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X] = 0x53;         // S
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y] = 0x41;         // A
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START] = 0xd;      // Enter
			break;
	}

	// reload
	PADSaveConfig(hwndDlg);
	PADLoadConfig(hwndDlg);
}

// ---------------------------------------------------------------------------

int GetVKey()
{
	int i;

	while (1)
	{
		for (i = 0; i < 0x80; i++)
		{
			if (GetAsyncKeyState(i) & 0x80000000)
			{
				if (i == VK_SHIFT || i == VK_CONTROL || i == VK_MENU) continue;  // rhyme :)
				if (i >= VK_F1 && i <= VK_F24) continue;
				if (i == VK_ESCAPE) return 0;
				else return i;
			}
		}
	}
}

INT_PTR CALLBACK PADConfigDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	wchar_t buf[256];
	int vkey;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		swprintf_s(buf, _countof(buf) - 1, L"Configure Controller %i", pad.padToConfigure + 1);
		SetWindowText(hwndDlg, buf);
		if (pad.padToConfigure != 0)
		{
			EnableWindow(GetDlgItem(hwndDlg, IDC_PAD_CONFIG_DEFAULT), FALSE);
		}
		PADLoadConfig(hwndDlg);
		return TRUE;

	case WM_CLOSE:
		EndDialog(hwndDlg, 0);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_UP:
			SetDlgItemText(hwndDlg, IDC_BUTTON_UP, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_UP, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_UP, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_DOWN:
			SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_LEFT:
			SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_RIGHT:
			SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_XUP50:
			SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_XUP100:
			SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_XDOWN50:
			SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_XDOWN100:
			SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_XLEFT50:
			SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_XLEFT100:
			SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_XRIGHT50:
			SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_XRIGHT100:
			SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_CXUP:
			SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_CXDOWN:
			SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_CXLEFT:
			SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_CXRIGHT:
			SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_TRIGGERL:
			SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_TRIGGERR:
			SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_TRIGGERZ:
			SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_A:
			SetDlgItemText(hwndDlg, IDC_BUTTON_A, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_A, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_A, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_B:
			SetDlgItemText(hwndDlg, IDC_BUTTON_B, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_B, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_B, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_X:
			SetDlgItemText(hwndDlg, IDC_BUTTON_X, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_X, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_X, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_Y:
			SetDlgItemText(hwndDlg, IDC_BUTTON_Y, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_Y, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_Y, GetVKDesc(vkey));
			return FALSE;

		case IDC_BUTTON_START:
			SetDlgItemText(hwndDlg, IDC_BUTTON_START, L"?");
			vkey = GetVKey();
			pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START] = vkey;
			if (vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_START, L"...");
			else SetDlgItemText(hwndDlg, IDC_BUTTON_START, GetVKDesc(vkey));
			return FALSE;

		case IDC_CHECK_PLUG:
			if (IsDlgButtonChecked(hwndDlg, IDC_CHECK_PLUG))
			{
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_UP), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWN), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_LEFT), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RIGHT), TRUE);

				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP50), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP100), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN50), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN100), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT50), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT100), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT50), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT100), TRUE);

				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXUP), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXDOWN), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXLEFT), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXRIGHT), TRUE);

				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERL), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERR), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERZ), TRUE);

				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_A), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_B), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_X), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_Y), TRUE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_START), TRUE);

				pad.config[pad.padToConfigure].plugged = true;
			}
			else
			{
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_UP), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWN), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_LEFT), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RIGHT), FALSE);

				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP50), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP100), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN50), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN100), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT50), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT100), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT50), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT100), FALSE);

				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXUP), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXDOWN), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXLEFT), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXRIGHT), FALSE);

				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERL), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERR), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERZ), FALSE);

				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_A), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_B), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_X), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_Y), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_START), FALSE);

				pad.config[pad.padToConfigure].plugged = false;
			}
			return FALSE;

		case IDC_PAD_CONFIG_CANCEL:
			EndDialog(hwndDlg, 0);
			return TRUE;

		case IDC_PAD_CONFIG_OK:
			PADSaveConfig(hwndDlg);
			EndDialog(hwndDlg, 0);
			return TRUE;

		case IDC_PAD_CONFIG_CLEAR:
			PADClearConfig(hwndDlg);
			return FALSE;

		case IDC_PAD_CONFIG_DEFAULT:
			PADDefaultConfig(hwndDlg);
			return FALSE;
		}
		break;

		default:
			return FALSE;
	}
	return FALSE;
}

void PADConfigure(long padnum, HWND hwndParent)
{
	pad.padToConfigure = padnum;

	DialogBox(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDD_DIALOG_PAD),
		hwndParent,
		PADConfigDialogProc);
}



/*

# Performance Counters

Interesting to track :
- The number of emulated Gekko instructions (million per second, mips)
- Number of recompiled and executed GekkoCore recompiler segments.
- Number of DSP instructions emulated (million per second, mips)
- Number of VI interrupts (frames per second)
- Number of draw operations (PE DrawDone / second)
- Show formatted value of TBR register (OSSystemTime)

*/

namespace UI
{

	// Global instance of the utility, which is controlled in the ui.cpp module
	PerfMetrics* g_perfMetrics = nullptr;


	void PerfMetrics::PerfThreadProc(void* param)
	{
		PerfMetrics* perf = (PerfMetrics*)param;

		// Get and reset counters

		int64_t gekkoMips = perf->GetGekkoInstructionsCounter();
		perf->ResetGekkoInstructionsCounter();

		int64_t compiledSegs = perf->GetGekkoCompiledSegments();
		perf->ResetGekkoCompiledSegments();

		int64_t executedSegs = perf->GetGekkoExecutedSegments();
		perf->ResetGekkoExecutedSegments();

		int64_t dspMips = perf->GetDspInstructionsCounter();
		perf->ResetDspInstructionsCounter();

		// If the number of executed segments is zero, then most likely the emulator is running in interpreter mode
		// or is in debug mode (emulation is temporarily stopped), so there is no point in displaying JITC statistics.

		char str[0x100];
		if (executedSegs != 0)
		{
			sprintf_s(str, sizeof(str), "gekko: %.02f mips (jitc %lld/%.02fM), dsp: %.02f mips",
				(float)gekkoMips / 1000000.f, compiledSegs, (float)executedSegs / 1000000.f, (float)dspMips / 1000000.f);
		}
		else
		{
			sprintf_s(str, sizeof(str), "gekko: %.02f mips, dsp: %.02f mips",
				(float)gekkoMips / 1000000.f, (float)dspMips / 1000000.f);
		}

		int32_t vis = perf->GetVICounter();
		perf->ResetVICounter();

		int32_t pes = perf->GetPECounter();
		perf->ResetPECounter();

		// Display information in the status bar

		SetStatusText(STATUS_ENUM::Progress, Util::StringToWstring(str));
		SetStatusText(STATUS_ENUM::VIs, std::to_wstring(vis) + L" VI/s");
		SetStatusText(STATUS_ENUM::PEs, std::to_wstring(pes) + L" PE/s");
		SetStatusText(STATUS_ENUM::SystemTime, Util::StringToWstring(perf->GetSystemTime()));

		Thread::Sleep(perf->metricsInterval);
	}

	PerfMetrics::PerfMetrics()
	{
		perfThread = EMUCreateThread(PerfThreadProc, false, this, "PerfThread");
	}

	PerfMetrics::~PerfMetrics()
	{
		EMUJoinThread(perfThread);
	}

	int64_t PerfMetrics::GetGekkoInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter(0);
	}

	void PerfMetrics::ResetGekkoInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter(0);
	}

	int64_t PerfMetrics::GetGekkoCompiledSegments()
	{
		return Jdi->GetPerformanceCounter(4);
	}

	void PerfMetrics::ResetGekkoCompiledSegments()
	{
		Jdi->ResetPerformanceCounter(4);
	}

	int64_t PerfMetrics::GetGekkoExecutedSegments()
	{
		return Jdi->GetPerformanceCounter(5);
	}

	void PerfMetrics::ResetGekkoExecutedSegments()
	{
		Jdi->ResetPerformanceCounter(5);
	}

	int64_t PerfMetrics::GetDspInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter(1);
	}

	void PerfMetrics::ResetDspInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter(1);
	}

	int32_t PerfMetrics::GetVICounter()
	{
		return (int32_t)Jdi->GetPerformanceCounter(2);
	}

	void PerfMetrics::ResetVICounter()
	{
		Jdi->ResetPerformanceCounter(2);
	}

	int32_t PerfMetrics::GetPECounter()
	{
		return (int32_t)Jdi->GetPerformanceCounter(3);
	}

	void PerfMetrics::ResetPECounter()
	{
		Jdi->ResetPerformanceCounter(3);
	}

	std::string PerfMetrics::GetSystemTime()
	{
		return Jdi->GetSystemTime();
	}

}




/* File selector. */

// Refactoring this code is bad idea :)

/* All important data is placed here */
UserSelector usel;

/* List of banners. */
static HIMAGELIST bannerList;

/* ---------------------------------------------------------------------------  */
/* PATH management                                                              */

/* Make sure path have ending '\\' */
static void fix_path(std::wstring& path)
{
	if (path.back() != L'\\')
	{
		path.push_back(L'\\');
	}
}

/* Remove all control symbols (below space). */
static void fix_string(wchar_t* str)
{
	for (size_t i = 0; i < wcslen(str); i++)
	{
		if (str[i] < L' ') str[i] = L' ';
	}
}

/* Add new path into "paths" list. */
static void add_path(std::wstring& path)
{
	// check path size
	size_t len = path.length() + 1;
	if (len >= MAX_PATH)
	{
		UI::Report(L"Too long path string: %s", path.c_str());
		return;
	}

	usel.paths.push_back(path);
}

// load PATH user variable and cut it on pieces into "paths" list
static void load_path()
{
	auto var = Util::StringToWstring(UI::Jdi->GetConfigString(USER_PATH, USER_UI));
	int n = 0;

	// delete current pathlist
	usel.paths.clear();

	// search new paths (separated by ';') until ending '\0'
	auto path = std::wstring();
	path.reserve(var.length());

	for (auto& c : var)
	{
		// add new one
		if (c == L';')
		{
			fix_path(path);
			add_path(path);
			path.clear();
			n++;
		}
		else
		{
			n++;
			path.push_back(c);
		}
	}

	// add last path
	if (n)
	{
		fix_path(path);
		add_path(path);
	}
}

/* Called after loading of new file */
bool AddSelectorPath(const std::wstring& fullPath)
{
	auto path = std::wstring(fullPath);

	if (fullPath.empty())
		return false;

	fix_path(path);

	bool exists = false;
	if (!usel.paths.empty())
	{
		auto itr = std::find(usel.paths.begin(), usel.paths.end(), path);
		exists = (itr != usel.paths.end());
	}

	if (!exists)
	{
		auto old = Util::StringToWstring(UI::Jdi->GetConfigString(USER_PATH, USER_UI));

		if (!old.empty())
		{
			path = fmt::format(L"{:s};{:s}", old, path);
		}

		UI::Jdi->SetConfigString(USER_PATH, Util::WstringToString(path), USER_UI);
		add_path(path);

		return true;
	}
	else
	{
		return false;
	}
}

/* ---------------------------------------------------------------------------  */
/* Add new file (extend filelist, convert banner, put it into list)             */

#define PACKRGB555(r, g, b) (uint16_t)((((r)&0xf8)<<7)|(((g)&0xf8)<<2)|(((b)&0xf8)>>3))
#define PACKRGB565(r, g, b) (uint16_t)((((r)&0xf8)<<8)|(((g)&0xfc)<<3)|(((b)&0xf8)>>3))

// convert banner. return indexes of new banner icon.
// we are using two icons for single banner, to highlight.
// return FALSE, if we cant convert banner (or something bad)
static bool add_banner(uint8_t* banner, int* bA, int* bB)
{
	int width = (usel.smallIcons) ? (DVD_BANNER_WIDTH >> 1) : (DVD_BANNER_WIDTH);
	int height = (usel.smallIcons) ? (DVD_BANNER_HEIGHT >> 1) : (DVD_BANNER_HEIGHT);
	HDC hdc = CreateDC(L"DISPLAY", NULL, NULL, NULL);
	int bitdepth = GetDeviceCaps(hdc, BITSPIXEL);
	int bpp = bitdepth / 8;
	int bcount = width * height * bpp;
	int tiles = (DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT) / 16;
	uint8_t* imageA, * imageB, * ptrA, * ptrB;
	double rgb[3], rgbh[3];
	double alpha, alphaC;
	uint16_t* tile = (uint16_t*)banner, * ptrA16, * ptrB16;
	uint32_t ri[4], gi[4], bi[4], ai2[4], rhi[4], ghi[4], bhi[4];     // for interpolation
	uint32_t r, g, b, a, rh, gh, bh;                                 // final values
	int row = 0, col = 0, pos;

	imageA = (uint8_t*)malloc(bcount);
	if (imageA == NULL)
	{
		DeleteDC(hdc);
		return FALSE;
	}
	imageB = (uint8_t*)malloc(bcount);
	if (imageB == NULL)
	{
		free(imageA);
		DeleteDC(hdc);
		return FALSE;
	}

	DWORD backcol = GetSysColor(COLOR_WINDOW);
	rgb[0] = (double)GetRValue(backcol);
	rgb[1] = (double)GetGValue(backcol);
	rgb[2] = (double)GetBValue(backcol);

	backcol = GetSysColor(COLOR_HIGHLIGHT);
	rgbh[0] = (double)GetRValue(backcol);
	rgbh[1] = (double)GetGValue(backcol);
	rgbh[2] = (double)GetBValue(backcol);

	// convert RGB5A3 -> RGBA
	for (int i = 0; i < tiles; i++, tile += 16)
	{
		for (int j = 0; j < 4; j += usel.smallIcons + 1)
			for (int k = 0; k < 4; k += usel.smallIcons + 1)
			{
				uint16_t p, ph;

				if (usel.smallIcons)     // small banner (interpolate)
				{
					for (int n = 0; n < 4; n++)
					{
						p = tile[(j + (n >> 1)) * 4 + (k + (n & 1))];
						p = (p << 8) | (p >> 8);

						if (p >> 15)
						{
							ri[n] = (p & 0x7c00) >> 10;
							gi[n] = (p & 0x03e0) >> 5;
							bi[n] = (p & 0x001f);
							ri[n] = (uint8_t)((ri[n] << 3) | (ri[n] >> 2));
							gi[n] = (uint8_t)((gi[n] << 3) | (gi[n] >> 2));
							bi[n] = (uint8_t)((bi[n] << 3) | (bi[n] >> 2));
							rhi[n] = ri[n], ghi[n] = gi[n], bhi[n] = bi[n];
						}
						else
						{
							ri[n] = (p & 0x0f00) >> 8;
							gi[n] = (p & 0x00f0) >> 4;
							bi[n] = (p & 0x000f);
							ai2[n] = (p & 0x7000) >> 12;

							alpha = (double)ai2[n] / 7.0;
							alphaC = 1.0 - alpha;

							rhi[n] = (uint8_t)((double)(16 * ri[n]) * alpha + rgbh[0] * alphaC);
							ghi[n] = (uint8_t)((double)(16 * gi[n]) * alpha + rgbh[1] * alphaC);
							bhi[n] = (uint8_t)((double)(16 * bi[n]) * alpha + rgbh[2] * alphaC);

							ri[n] = (uint8_t)((double)(16 * ri[n]) * alpha + rgb[0] * alphaC);
							gi[n] = (uint8_t)((double)(16 * gi[n]) * alpha + rgb[1] * alphaC);
							bi[n] = (uint8_t)((double)(16 * bi[n]) * alpha + rgb[2] * alphaC);
						}
					}

					// bilinear interpolation
					r = ((ri[0] + ri[1] + ri[2] + ri[3]) >> 2);
					g = ((gi[0] + gi[1] + gi[2] + gi[3]) >> 2);
					b = ((bi[0] + bi[1] + bi[2] + bi[3]) >> 2);
					rh = ((rhi[0] + rhi[1] + rhi[2] + rhi[3]) >> 2);
					gh = ((ghi[0] + ghi[1] + ghi[2] + ghi[3]) >> 2);
					bh = ((bhi[0] + bhi[1] + bhi[2] + bhi[3]) >> 2);

					pos = bpp * (((row + j) >> 1) * width + ((col + k) >> 1));
				}
				else                    // large banner
				{
					p = tile[j * 4 + k];
					ph = p = (p << 8) | (p >> 8);
					if (p >> 15)
					{
						r = (p & 0x7c00) >> 10;
						g = (p & 0x03e0) >> 5;
						b = (p & 0x001f);
						r = (uint8_t)((r << 3) | (r >> 2));
						g = (uint8_t)((g << 3) | (g >> 2));
						b = (uint8_t)((b << 3) | (b >> 2));
						rh = r, gh = g, bh = b;
					}
					else
					{
						r = (p & 0x0f00) >> 8;
						g = (p & 0x00f0) >> 4;
						b = (p & 0x000f);
						a = (p & 0x7000) >> 12;

						alpha = (double)a / 7.0;
						alphaC = 1.0 - alpha;

						rh = (uint8_t)((double)(16 * r) * alpha + rgbh[0] * alphaC);
						gh = (uint8_t)((double)(16 * g) * alpha + rgbh[1] * alphaC);
						bh = (uint8_t)((double)(16 * b) * alpha + rgbh[2] * alphaC);

						r = (uint8_t)((double)(16 * r) * alpha + rgb[0] * alphaC);
						g = (uint8_t)((double)(16 * g) * alpha + rgb[1] * alphaC);
						b = (uint8_t)((double)(16 * b) * alpha + rgb[2] * alphaC);
					}

					pos = bpp * ((row + j) * width + (col + k));
				}

				ptrA16 = (uint16_t*)&imageA[pos];
				ptrB16 = (uint16_t*)&imageB[pos];
				ptrA = &imageA[pos];
				ptrB = &imageB[pos];

				if (bitdepth == 8)
				{
					// you can test 8-bit in XP, running in 256 colors
					*ptrA++ =
						*ptrB++ = (uint8_t)(r | g ^ b);
				}
				else if (bitdepth == 16)
				{
					p = PACKRGB565(r, g, b);
					ph = PACKRGB565(rh, gh, bh);

					*ptrA16 = p;
					*ptrB16 = ph;
				}
				else if (bitdepth == 24)
				{
					*ptrA++ = (uint8_t)b;
					*ptrA++ = (uint8_t)g;
					*ptrA++ = (uint8_t)r;

					*ptrB++ = (uint8_t)bh;
					*ptrB++ = (uint8_t)gh;
					*ptrB++ = (uint8_t)rh;
				}
				else    // 32 bpp ARGB format.
				{
					*ptrA++ = (uint8_t)b;
					*ptrA++ = (uint8_t)g;
					*ptrA++ = (uint8_t)r;
					*ptrA++ = (uint8_t)255;

					*ptrB++ = (uint8_t)bh;
					*ptrB++ = (uint8_t)gh;
					*ptrB++ = (uint8_t)rh;
					*ptrB++ = (uint8_t)255;
				}
			}

		col += 4;
		if (col == DVD_BANNER_WIDTH)
		{
			col = 0;
			row += 4;
		}
	}

	// add new icons
	HBITMAP hbm = CreateCompatibleBitmap(hdc, width, height);
	if (hbm == NULL)
	{
		DeleteDC(hdc);
		free(imageA), free(imageB);
		return FALSE;
	}

	SetBitmapBits(hbm, bcount, imageA);     // normal icon
	*bA = ImageList_Add(bannerList, hbm, NULL);

	SetBitmapBits(hbm, bcount, imageB);     // highlighted icon
	*bB = ImageList_Add(bannerList, hbm, NULL);

	// clean up
	DeleteObject(hbm);
	DeleteDC(hdc);
	free(imageA);
	free(imageB);
	return TRUE;
}

// add new item
static void add_item(size_t index)
{
	LV_ITEM lvi = { 0 };
	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	lvi.iItem = ListView_GetItemCount(usel.hSelectorWindow);
	lvi.lParam = (LPARAM)index;
	lvi.pszText = LPSTR_TEXTCALLBACK;
	ListView_InsertItem(usel.hSelectorWindow, &lvi);
}

static void CopyAnsiStringAsWcharString(wchar_t* dest, const char* src)
{
	char* ansiPtr = (char*)src;
	wchar_t* wcharPtr = (wchar_t*)dest;
	while (*ansiPtr)
	{
		*wcharPtr++ = (uint8_t)*ansiPtr++;
	}
	*wcharPtr++ = 0;
}

/* Insert new file into filelist. */
static void add_file(const std::wstring& file, int fsize, SELECTOR_FILE type)
{
	// check file size
	if ((fsize < 0x1000) || (fsize > DVD_SIZE))
	{
		return;
	}

	// check already present
	bool found = false;

	if (!usel.files.empty())
	{
		auto itr = std::find_if(usel.files.begin(), usel.files.end(), [&](auto& entry)
			{
				return entry->name == file;
			});

		found = (itr != usel.files.end());
	}

	if (found)
	{
		return;
	}

	/* Try to open file */
	if (!Util::FileExists(file.c_str()))
	{
		return;
	}

	auto item = std::make_unique<UserFile>();

	/* Save file info */
	item->type = type;
	item->size = fsize;
	item->name = file;

	item->name.resize(2 * MAX_PATH + 2);
	if (type == SELECTOR_FILE::Dvd)
	{
		// To get information from the disk, you have to remount it. 
		// If the user, for example, mounted DolphinSDK, it is necessary to restore the previous state of the mount so that he does not get upset.

		bool mounted = false;
		std::string path;
		bool mountedAsIso = false;

		/* Load DVD banner. */
		std::vector<uint8_t> banner = DVDLoadBanner(file.c_str());
		if (banner.empty())
		{
			return;
		}

		// Keep previous mount state
		mounted = UI::Jdi->DvdIsMounted(path, mountedAsIso);

		// get DiskID
		std::vector<uint8_t> diskIDRaw;
		diskIDRaw.resize(4);
		wchar_t diskID[0x10] = { 0 };
		UI::Jdi->DvdMount(Util::WstringToString(file));
		UI::Jdi->DvdSeek(0);
		UI::Jdi->DvdRead(diskIDRaw);
		diskID[0] = diskIDRaw[0];
		diskID[1] = diskIDRaw[1];
		diskID[2] = diskIDRaw[2];
		diskID[3] = diskIDRaw[3];
		diskID[4] = 0;

		/* Set GameID. */
		item->id = fmt::sprintf(L"%.4s", diskID);

		// Restore previous mount state
		if (mounted)
		{
			if (mountedAsIso)
			{
				UI::Jdi->DvdMount(path);
			}
			else
			{
				UI::Jdi->DvdMountSDK(path);
			}
		}
		else
		{
			UI::Jdi->DvdUnmount();
		}

		/* Use banner info and remove line-feeds. */
		DVDBanner2* bnr = (DVDBanner2*)banner.data();
		CopyAnsiStringAsWcharString(item->title, (char*)bnr->comments[0].longTitle);
		CopyAnsiStringAsWcharString(item->comment, (char*)bnr->comments[0].comment);
		fix_string(item->title);
		fix_string(item->comment);

		// convert banner to RGB and add into bannerlist
		int a, b;
		bool res = add_banner(bnr->image, &a, &b);
		if (res == false)
		{
			return;    // we cant :(
		}

		item->icon[0] = a;
		item->icon[1] = b;
	}
	else if (type == SELECTOR_FILE::Executable)
	{
		/* Rip filename */
		wchar_t drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];

		_wsplitpath_s(file.c_str(),
			drive, _countof(drive) - 1,
			dir, _countof(dir) - 1,
			name, _countof(name) - 1,
			ext, _countof(ext) - 1);

		item->id = L"-";
		wcscpy_s(item->title, _countof(item->title) - 1, name);
		item->comment[0] = 0;
	}
	else
	{
		assert(0);
	}

	/* Extend filelist. */
	usel.files.push_back(std::move(item));

	/* Add listview item. */
	add_item(usel.files.size() - 1);
}

// ---------------------------------------------------------------------------
// draw

// we are using selector "width" variable to adjust last column
static void reset_columns()
{
	LV_COLUMN   lvcol;

	// delete old columns
	memset(&lvcol, 0, sizeof(lvcol));
	lvcol.mask = LVCF_FMT;
	while (ListView_GetColumn(usel.hSelectorWindow, 0, &lvcol))
	{
		ListView_DeleteColumn(usel.hSelectorWindow, 0);
	}

	lvcol.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

	// add new columns
	#define ADDCOL(align, width, text, id)                          \
	{                                                               \
		lvcol.fmt = LVCFMT_##align;                                 \
		lvcol.cx  = width;                                          \
		lvcol.pszText = text;                                       \
		ListView_InsertColumn(usel.hSelectorWindow, id, &lvcol);    \
	}

	ADDCOL(LEFT, (usel.smallIcons) ? (57) : (105), (wchar_t*)SELECTOR_COLUMN_BANNER, 0);
	ADDCOL(LEFT, 200, (wchar_t*)SELECTOR_COLUMN_TITLE, 1);
	ADDCOL(CENTER, 60, (wchar_t*)SELECTOR_COLUMN_SIZE, 2);
	ADDCOL(CENTER, 60, (wchar_t*)SELECTOR_COLUMN_GAMEID, 3);

	// last one is tricky
	int commentWidth = usel.width -
		(((usel.smallIcons) ? (57) : (105)) + 200 + 60 + 60) -
		GetSystemMetrics(SM_CXVSCROLL) - 4;
	if (commentWidth < 190) commentWidth = 190;
	ADDCOL(LEFT, commentWidth, (wchar_t*)SELECTOR_COLUMN_COMMENT, 4);
}

void ResizeSelector(int width, int height)
{
	// opened ?
	if (!usel.opened) return;

	if (IsWindow(usel.hSelectorWindow))
	{
		// adjust
		if (IsWindow(wnd.hStatusWindow))
		{
			RECT rc;
			GetWindowRect(wnd.hStatusWindow, &rc);
			height -= (WORD)(rc.bottom - rc.top);
		}

		// move window
		MoveWindow(usel.hSelectorWindow, 0, 0, width, height, TRUE);
		usel.width = width;
		usel.height = height;

		reset_columns();
	}
}

uint16_t* SjisToUnicode(wchar_t* sjisText, size_t* size, size_t* chars)
{
	uint16_t* unicodeText, * ptrU, uchar, schar;
	wchar_t* ptrS;

	*size = (wcslen(sjisText) + 1) * sizeof(wchar_t);
	unicodeText = (uint16_t*)malloc(*size);
	assert(unicodeText);
	memset(unicodeText, 0, *size);

	ptrU = unicodeText;
	ptrS = sjisText;
	*chars = 0;

	schar = *ptrS;
	while (schar != 0)
	{
		uchar = SjisTable[schar];
		if (uchar == 0xFFFF)
		{
			ptrS++;
			schar = (schar << 8) | *ptrS;
			uchar = SjisTable[schar];
		}
		*ptrU = uchar;

		ptrU++;
		ptrS++;
		(*chars)++;
		schar = *ptrS;
	}
	return unicodeText;
}

// draw single item
void DrawSelectorItem(LPDRAWITEMSTRUCT item)
{
	// opened ?
	if (!usel.opened) return;

#define     ID item->itemID
#define     DC item->hDC
	UserFile* file;       // item to draw
	bool        selected;   // 1, if item is selected
	HBRUSH      hb;         // background brush
	LV_ITEM     lvi;
	LV_COLUMN   lvc;
	LVCOLUMNW   lvcw;
	RECT        rc, rc2;

	// get selected item
	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_PARAM;
	lvi.iItem = ID;
	if (!ListView_GetItem(usel.hSelectorWindow, &lvi)) return;
	lvi.state = ListView_GetItemState(usel.hSelectorWindow, ID, -1);
	selected = (lvi.state & LVIS_SELECTED) ? (TRUE) : (FALSE);
	file = usel.files[lvi.lParam].get();

	// select background brush
	if (selected)
	{
		hb = (HBRUSH)(COLOR_HIGHLIGHT + 1);
		SetTextColor(DC, GetSysColor(COLOR_HIGHLIGHTTEXT));
	}
	else
	{
		hb = (HBRUSH)(COLOR_WINDOW + 1);
		SetTextColor(DC, GetSysColor(COLOR_WINDOWTEXT));
	}

	if (selected)
	{
		SetStatusText(STATUS_ENUM::Progress, file->name);
	}

	// fill background
	FillRect(DC, &item->rcItem, hb);
	SetBkMode(DC, TRANSPARENT);

	// draw file icon
	ListView_GetItemRect(usel.hSelectorWindow, ID, &rc, LVIR_ICON);
	switch (file->type)
	{
		case SELECTOR_FILE::Dvd:
			ImageList_Draw(bannerList, file->icon[selected], DC,
				rc.left, rc.top, ILD_NORMAL);
			break;

		case SELECTOR_FILE::Executable:
			HICON hIcon;
			if (usel.smallIcons)
			{
				hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_GCN_SMALL_ICON));
				DrawIcon(DC, (rc.right - rc.left) / 2 - 4, rc.top, hIcon);
			}
			else
			{
				hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_GCN_ICON));
				DrawIcon(DC, (rc.right - rc.left) / 2 - 16, rc.top, hIcon);
			}
			DeleteObject(hIcon);
			break;
	}

	// other columns
	ListView_GetItemRect(usel.hSelectorWindow, ID, &rc, LVIR_LABEL);
	memset(&lvc, 0, sizeof(lvc));
	lvc.mask = LVCF_FMT | LVCF_WIDTH;

	for (int col = 1; ListView_GetColumn(usel.hSelectorWindow, col, &lvc); col++)
	{
		wchar_t text[0x1000] = { 0, };
		UINT fmt = DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER;
		lvcw.mask = LVCF_FMT;
		ListView_GetColumn(usel.hSelectorWindow, col, &lvcw);

		if (lvcw.fmt & LVCFMT_RIGHT)       fmt |= DT_RIGHT;
		else if (lvcw.fmt & LVCFMT_CENTER) fmt |= DT_CENTER;
		else                              fmt |= DT_LEFT;

		rc.left = rc.right;
		rc.right += lvc.cx;

		ListView_GetItemText(usel.hSelectorWindow, ID, col, text, _countof(text) - 1);
		size_t len = wcslen(text);

		rc2 = rc;
		FillRect(DC, &rc2, hb);
		rc2.left += 2;
		rc2.right -= 2;

		if (file->type == SELECTOR_FILE::Executable)
		{
			DrawText(DC, text, (int)len, &rc2, fmt);
			continue;
		}

		char DiskId[4] = { 0 };
		DiskId[0] = (char)file->id[0];
		DiskId[1] = (char)file->id[1];
		DiskId[2] = (char)file->id[2];
		DiskId[3] = (char)file->id[3];

		std::string regionName = UI::Jdi->DvdRegionById(DiskId);

		if (regionName == "JPN" &&
			(col == 1 || col == 4))     // title or comment only
		{
			uint16_t* buf; size_t size, chars;
			buf = SjisToUnicode(text, &size, &chars);
			DrawTextW(DC, (wchar_t*)buf, (int)chars, &rc2, fmt);
			free(buf);
		}
		else
		{
			DrawText(DC, text, (int)len, &rc2, fmt);
		}
	}

#undef ID
#undef DC
}

// update filelist (reload and redraw)
void UpdateSelector()
{
	static std::vector<std::pair<std::string, SELECTOR_FILE>> file_ext =
	{
		{ ".dol", SELECTOR_FILE::Executable },
		{ ".elf", SELECTOR_FILE::Executable },
		{ ".gcm", SELECTOR_FILE::Dvd        },
		{ ".iso", SELECTOR_FILE::Dvd        }
	};

	wchar_t search[2 * MAX_PATH];
	const wchar_t* mask[] = { L"*.dol", L"*.elf", L"*.gcm", L"*.iso", NULL };
	SELECTOR_FILE type[] = { SELECTOR_FILE::Executable, SELECTOR_FILE::Executable, SELECTOR_FILE::Dvd, SELECTOR_FILE::Dvd };
	WIN32_FIND_DATA fd = { 0 };
	HANDLE hfff;
	wchar_t found[2 * MAX_PATH];

	/* Opened? */
	if (!usel.opened || usel.updateInProgress)
	{
		return;
	}

	usel.updateInProgress = true;

	/* Destroy old filelist and path data */
	ListView_DeleteAllItems(usel.hSelectorWindow);

	usel.files.clear();

	ImageList_Remove(bannerList, -1);

	// build search path list (even if selector closed)
	load_path();
	//list_path();

	// load file filter
	usel.filter = UI::Jdi->GetConfigInt(USER_FILTER, USER_UI);

	// search all directories
	size_t dir = 0;
	while (dir < usel.paths.size())
	{
		int m = 0;
		uint32_t filter = _byteswap_ulong(usel.filter);

		while (mask[m])
		{
			uint8_t allow = (uint8_t)filter;
			filter >>= 8;
			if (!allow) { m++; continue; }

			swprintf_s(search, _countof(search), L"%s%s", usel.paths[dir].c_str(), mask[m]);

			memset(&fd, 0, sizeof(fd));

			hfff = FindFirstFile(search, &fd);
			if (hfff != INVALID_HANDLE_VALUE)
			{
				do
				{
					if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
					{
						swprintf_s(found, _countof(found), L"%s%s", usel.paths[dir].c_str(), fd.cFileName);
						// Add file in list
						add_file(found, fd.nFileSizeLow, type[m]);
					}
				} while (FindNextFile(hfff, &fd));
			}
			FindClose(hfff);

			// next mask
			m++;
		}

		// next directory
		dir++;
	}

	/* Update selector window */
	UpdateWindow(usel.hSelectorWindow);

	/* Re-sort (we need to save old value, to avoid swap of filelist) */
	SELECTOR_SORT oldSort = usel.sortBy;
	usel.sortBy = SELECTOR_SORT::Unsorted;
	SortSelector(oldSort);

	usel.updateInProgress = false;
}

int SelectorGetSelected()
{
	int item = ListView_GetNextItem(usel.hSelectorWindow, -1, LVNI_SELECTED);
	if (item == -1) return -1;
	return item;
}

void SelectorSetSelected(size_t item)
{
	if (item >= usel.files.size()) return;
	ListView_SetItemState(usel.hSelectorWindow, item, LVNI_SELECTED, LVNI_SELECTED);
	ListView_EnsureVisible(usel.hSelectorWindow, item, FALSE);
}

// if file not present, keep selection unchanged
void SelectorSetSelected(const std::wstring& filename)
{
	for (size_t i = 0; i < usel.files.size(); i++)
	{
		if (filename == usel.files[i]->name)
		{
			SelectorSetSelected(i);
			break;
		}
	}
}

/* ---------------------------------------------------------------------------  */
/* Controls                                                                     */

/* Return item string data. */
static void getdispinfo(LPNMHDR pnmh)
{
	LV_DISPINFO* lpdi = (LV_DISPINFO*)pnmh;

	if (lpdi->item.lParam < 0)
		return;

	auto file = usel.files[lpdi->item.lParam].get();
	auto wcharStr = std::wstring();

	switch (lpdi->item.iSubItem)
	{
		case 0:
			wcharStr = L" ";
			break;
		case 1:         /* Title */
			wcharStr = file->title;
			break;
		case 2:         /* Length */
			wcharStr = UI::FileSmartSize(file->size);
			break;
		case 3:         /* ID */
			wcharStr = file->id;
			break;
		case 4:         /* Comment */
			wcharStr = file->comment;
			break;
		default:
			break;
	}

	if (!wcharStr.empty())
	{
		wcsncpy_s(lpdi->item.pszText, lpdi->item.cchTextMax, wcharStr.data(), wcharStr.length());
	}
}

static void columnclick(int col)
{
	switch (col)
	{
		case 0: SortSelector(SELECTOR_SORT::Default); break;
		case 1: SortSelector(SELECTOR_SORT::Title); break;
		case 2: SortSelector(SELECTOR_SORT::Size); break;
		case 3: SortSelector(SELECTOR_SORT::ID); break;
		case 4: SortSelector(SELECTOR_SORT::Comment); break;
	}
}

static void mouseclick(int rmb)
{
	int item = ListView_GetNextItem(usel.hSelectorWindow, -1, LVNI_SELECTED);

	if (usel.files.size() == 0 || item < 0)
		return;

	UserFile* file = usel.files[item].get();

	if (item == -1)          // empty field
	{
		SetStatusText(STATUS_ENUM::Progress, L"Idle");
	}
	else                    // file selected
	{
		// show item
		SetStatusText(STATUS_ENUM::Progress, file->name.data());
	}
}

static void doubleclick()
{
	int item = ListView_GetNextItem(usel.hSelectorWindow, -1, LVNI_SELECTED);

	if (usel.files.size() == 0 || item < 0)
		return;

	std::wstring path(usel.files[item]->name);

	UI::Jdi->Unload();
	UI::Jdi->LoadFile(Util::WstringToString(path));
	if (Debug::debugger)
	{
		Debug::debugger->InvalidateAll();
	}
	OnMainWindowOpened(path.c_str());
	UI::Jdi->Run();
}

void NotifySelector(LPNMHDR pnmh)
{
	// opened ?
	if (!usel.opened) return;

	switch (pnmh->code)
	{
		case LVN_COLUMNCLICK: columnclick(((NM_LISTVIEW*)pnmh)->iSubItem); break;
		case LVN_GETDISPINFO: getdispinfo(pnmh); break;
		case NM_CLICK: mouseclick(0); break;
		case NM_RCLICK: mouseclick(1); break;
		case NM_RETURN: doubleclick(); break;
		case NM_DBLCLK: doubleclick(); break;
	}
}

// set selected item, by first letter key pressed
void ScrollSelector(int letter)
{
	letter = tolower(letter);
	for (size_t n = 0; n < usel.files.size(); n++)
	{
		UserFile* file = usel.files[n].get();
		int c = tolower(file->title[0]);
		if (c == letter)
		{
			SelectorSetSelected(n);
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// sort (using C qsort() function)

static int sort_by_type(const void* cmp1, const void* cmp2)
{
	UserFile* f1 = (UserFile*)cmp1, * f2 = (UserFile*)cmp2;
	return ((int)f2->type - (int)f1->type);
}

static int sort_by_filename(const void* cmp1, const void* cmp2)
{
	UserFile* f1 = (UserFile*)cmp1, * f2 = (UserFile*)cmp2;
	return _wcsicmp(f1->name.data(), f2->name.data());
}

static int sort_by_title(const void* cmp1, const void* cmp2)
{
	UserFile* f1 = (UserFile*)cmp1, * f2 = (UserFile*)cmp2;
	return _wcsicmp(f1->title, f2->title);
}

static int sort_by_size(const void* cmp1, const void* cmp2)
{
	UserFile* f1 = (UserFile*)cmp1, * f2 = (UserFile*)cmp2;
	return (int)(f1->size - f2->size);
}

static int sort_by_gameid(const void* cmp1, const void* cmp2)
{
	UserFile* f1 = (UserFile*)cmp1, * f2 = (UserFile*)cmp2;
	return wcscmp(f1->id.data(), f2->id.data());
}

static int sort_by_comment(const void* cmp1, const void* cmp2)
{
	UserFile* f1 = (UserFile*)cmp1, * f2 = (UserFile*)cmp2;
	return _wcsicmp(f1->comment, f2->comment);
}

// count DVD files in list
static int get_dvd_files()
{
	int sum = 0;
	for (size_t i = 0; i < usel.files.size(); i++)
	{
		UserFile* file = usel.files[i].get();
		if (file->type == SELECTOR_FILE::Dvd) sum++;
	}
	return sum;
}

void SortSelector(SELECTOR_SORT sortBy)
{
	// opened ?
	if (!usel.opened) return;

	// sort
	if (usel.sortBy != sortBy)
	{
		int dvds = get_dvd_files();

		switch (sortBy)
		{
			case SELECTOR_SORT::Default:
				//qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_type);
				//qsort(usel.files, dvds, sizeof(UserFile), sort_by_title);
				//qsort(&usel.files[dvds], usel.filenum-dvds, sizeof(UserFile), sort_by_title);
				break;
			case SELECTOR_SORT::Filename:
				//qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_filename);
				break;
			case SELECTOR_SORT::Title:
				//qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_title);
				break;
			case SELECTOR_SORT::Size:
				//qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_size);
				break;
			case SELECTOR_SORT::ID:
				//qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_gameid);
				break;
			case SELECTOR_SORT::Comment:
				//qsort(usel.files, usel.filenum, sizeof(UserFile), sort_by_comment);
				break;
			default:
				break;
		}

		usel.sortBy = sortBy;
		UI::Jdi->SetConfigInt(USER_SORTVIEW, (int)usel.sortBy, USER_UI);
	}
	else
	{
		// swap it, if same sort method
		//for(int i=0; i<usel.filenum/2; i++)
		//{
		//    UserFile tmp = usel.files[i];
		//    usel.files[i]  = usel.files[usel.filenum-i-1];
		//    usel.files[usel.filenum-i-1] = tmp;
		//}
	}

	// rebuild filelist
	ListView_DeleteAllItems(usel.hSelectorWindow);
	for (size_t i = 0; i < usel.files.size(); i++) add_item(i);
}

// ---------------------------------------------------------------------------
// management (create/close selector)

// two flags are controlling selector view : "active" and "opened". selector
// cannot be opened, if it is not active. therefore, create/close calls
// should check for "active" flag first, then check "opened" flag. there is no
// need to check "active" flags for other calls, because if it is not "active"
// it cannot be "opened".

void CreateSelector()
{
	// allowed ?
	if (!usel.active)
	{
		load_path();
		return;
	}

	// already created ?
	if (usel.opened) return;

	usel.updateInProgress = false;

	HWND parent = wnd.hMainWindow;
	HINSTANCE hinst = GetModuleHandle(NULL);
	InitCommonControls();

	// create selector window
	usel.hSelectorWindow = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER |
		LVS_OWNERDRAWFIXED | LVS_SINGLESEL | LVS_REPORT,
		0, 0, 0, 0,
		parent, (HMENU)ID_SELECTOR, hinst, NULL);
	if (usel.hSelectorWindow == NULL) return;

	EnableWindow(usel.hSelectorWindow, TRUE);
	ShowWindow(usel.hSelectorWindow, SW_SHOW);
	SetFocus(usel.hSelectorWindow);

	// retrieve icon size
	bool iconSize = UI::Jdi->GetConfigBool(USER_SMALLICONS, USER_UI);

	// set "opened" flag (for following calls)
	usel.opened = TRUE;

	// update selector view, by changing icon size
	SetSelectorIconSize(iconSize);

	// sort files
	usel.sortBy = SELECTOR_SORT::Unsorted;
	SortSelector((SELECTOR_SORT)UI::Jdi->GetConfigInt(USER_SORTVIEW, USER_UI));

	// scroll to last loaded file
	SelectorSetSelected(Util::StringToWstring(UI::Jdi->GetConfigString(USER_LASTFILE, USER_UI)));
}

void CloseSelector()
{
	// allowed ?
	if (!usel.active) return;

	// already closed ?
	if (!usel.opened) return;

	// destroy filelist
	usel.files.clear();

	// destroy bannerlist
	ImageList_Remove(bannerList, -1);
	ImageList_Destroy(bannerList);
	bannerList = NULL;

	// destroy window
	ListView_DeleteAllItems(usel.hSelectorWindow);
	DestroyWindow(usel.hSelectorWindow);
	usel.hSelectorWindow = NULL;
	SetFocus(wnd.hMainWindow);

	// clear "opened" flag
	usel.opened = FALSE;
}

// 0: large, 1:small
void SetSelectorIconSize(bool smallIcon)
{
	// opened ?
	if (!usel.opened) return;

	usel.smallIcons = smallIcon;
	UI::Jdi->SetConfigBool(USER_SMALLICONS, usel.smallIcons, USER_UI);

	// destroy bannerlist
	if (bannerList)
	{
		ImageList_Remove(bannerList, -1);
		ImageList_Destroy(bannerList);
		bannerList = NULL;
	}

	// create banners imagelist
	if (bannerList == NULL)
	{
		int w = (usel.smallIcons) ? (DVD_BANNER_WIDTH >> 1) : (DVD_BANNER_WIDTH);
		int h = (usel.smallIcons) ? (DVD_BANNER_HEIGHT >> 1) : (DVD_BANNER_HEIGHT);
		bannerList = ImageList_Create(w, h, ILC_COLOR24, 10, 10);
		assert(bannerList);
		ListView_SetImageList(usel.hSelectorWindow, bannerList, LVSIL_SMALL);
	}

	// resize and update
	RECT rc;
	GetClientRect(wnd.hMainWindow, &rc);
	ResizeSelector((WORD)(rc.right - rc.left),
		(WORD)(rc.bottom - rc.top));
	UpdateSelector();
}


// ---------------------------------------------------------------------------
// Settings dialog (to configure user variables)

// all user variables (except memory cards vars) are placed in config.h

// parent window and instance
static HWND         hParentWnd, hChildDlg[2];
static HINSTANCE    hParentInst;
static BOOL         settingsLoaded[2];
static BOOL         needSelUpdate;

static const wchar_t* tabs[] =
{
	L"GUI/Selector",
	L"GCN Hardware",
};

static struct ConsoleVersion
{
	uint32_t ver;
	const wchar_t* info;
} consoleVersion[] = {
	{ 0x00000001, L"0x00000001: Retail 1" },
	{ 0x00000002, L"0x00000002: HW2 production board" },
	{ 0x00000003, L"0x00000003: The latest production board" },
	{ 0x10000004, L"0x10000004: 1st Devkit HW" },
	{ 0x10000005, L"0x10000005: 2nd Devkit HW" },
	{ 0x10000006, L"0x10000006: The latest Devkit HW" },
	{ 0xffffffff, L"0x%08X: User defined" }
};

static char* int2str(int i)
{
	static char str[16];
	sprintf_s(str, sizeof(str), "%i", i);
	return str;
}

static void LoadSettings(int n)         // dialogs created
{
	HWND hDlg = hChildDlg[n];

	// GUI/Selector
	if (n == 0)
	{
		for (auto it = usel.paths.begin(); it != usel.paths.end(); ++it)
		{
			std::wstring path = *it;
			SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_ADDSTRING, 0, (LPARAM)path.data());
		}

		needSelUpdate = FALSE;
		settingsLoaded[n] = TRUE;
	}

	// GCN Hardware
	if (n == 1)
	{
		uint32_t ver = UI::Jdi->GetConfigInt(USER_CONSOLE, USER_HW);
		int i = 0, selected = -1;
		while (consoleVersion[i].ver != 0xffffffff)
		{
			if (consoleVersion[i].ver == ver)
			{
				selected = i;
				break;
			}
			i++;
		}
		if (consoleVersion[i].ver == 0xffffffff) selected = -1;
		i = 0;
		SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_RESETCONTENT, 0, 0);
		do
		{
			SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_INSERTSTRING, -1, (LPARAM)consoleVersion[i].info);
		} while (consoleVersion[++i].ver != 0xffffffff);
		if (selected == -1)
		{
			wchar_t buf[100];
			swprintf_s(buf, _countof(buf) - 1, consoleVersion[selected].info, ver);
			SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_INSERTSTRING, -1, (LPARAM)buf);
		}
		SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_SETCURSEL, selected, 0);

		SetDlgItemText(hDlg, IDC_BOOTROM_FILE, Util::StringToWstring(UI::Jdi->GetConfigString(USER_BOOTROM, USER_HW)).c_str());
		SetDlgItemText(hDlg, IDC_DSPDROM_FILE, Util::StringToWstring(UI::Jdi->GetConfigString(USER_DSP_DROM, USER_HW)).c_str());
		SetDlgItemText(hDlg, IDC_DSPIROM_FILE, Util::StringToWstring(UI::Jdi->GetConfigString(USER_DSP_IROM, USER_HW)).c_str());

		settingsLoaded[n] = TRUE;
	}
}

static void SaveSettings()              // OK pressed
{
	int i;
	auto buf = std::wstring(0x1000, 0);

	/* GUI/Selector. */
	if (settingsLoaded[0])
	{
		HWND hDlg = hChildDlg[0];
		int max = (int)SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCOUNT, 0, 0);
		static wchar_t text_buffer[1024];

		/* Delete all directories. */
		usel.paths.clear();
		UI::Jdi->SetConfigString(USER_PATH, "", USER_UI);

		/* Add directories again. */
		for (i = 0; i < max; i++)
		{
			SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETTEXT, i, (LPARAM)text_buffer);

			/* Add the path. */
			std::wstring wstr = text_buffer;
			AddSelectorPath(wstr);
		}

		/* Update selector layout, if PATH has changed */
		if (needSelUpdate)
		{
			UpdateSelector();
			needSelUpdate = FALSE;
		}
	}

	// GCN Hardwre
	if (settingsLoaded[1])
	{
		HWND hDlg = hChildDlg[1];
		int selected = (int)SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_GETCURSEL, 0, 0);
		UI::Jdi->SetConfigInt(USER_CONSOLE, consoleVersion[selected].ver, USER_HW);

		GetDlgItemText(hDlg, IDC_BOOTROM_FILE, (LPWSTR)buf.data(), (int)buf.size());
		UI::Jdi->SetConfigString(USER_BOOTROM, Util::WstringToString(buf), USER_HW);
		GetDlgItemText(hDlg, IDC_DSPDROM_FILE, (LPWSTR)buf.data(), (int)buf.size());
		UI::Jdi->SetConfigString(USER_DSP_DROM, Util::WstringToString(buf), USER_HW);
		GetDlgItemText(hDlg, IDC_DSPIROM_FILE, (LPWSTR)buf.data(), (int)buf.size());
		UI::Jdi->SetConfigString(USER_DSP_IROM, Util::WstringToString(buf), USER_HW);
	}
}


// UserMenu
static INT_PTR CALLBACK UserMenuSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	int i;
	int curSel, max;
	std::wstring path, text(1024, 0);

	switch (message)
	{
		case WM_INITDIALOG:
			hChildDlg[0] = hDlg;
			LoadSettings(0);
			return true;

		case WM_COMMAND:
			switch (wParam)
			{
				case IDC_FILEFILTER:
				{
					EditFileFilter(hDlg);
					break;
				}
				case IDC_ADDPATH:
				{
					path = UI::FileOpenDialog(UI::FileType::Directory);
					if (!path.empty())
					{
						fix_path(path);

						/* Check if already present. */
						max = (int)SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCOUNT, 0, 0);
						for (i = 0; i < max; i++)
						{
							SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETTEXT, i, (LPARAM)text.data());
							if (path == text) break;
						}

						/* Add new path. */
						if (i == max)
						{
							SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_ADDSTRING, 0, (LPARAM)path.data());
							needSelUpdate = TRUE;
						}
					}

					break;
				}
				case IDC_KILLPATH:
				{
					curSel = (int)SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCURSEL, 0, 0);
					SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_DELETESTRING, (WPARAM)curSel, 0);
					needSelUpdate = TRUE;

					break;
				}
			}
			break;

		case WM_NOTIFY:
		{
			if (((NMHDR FAR*)lParam)->code == PSN_APPLY) SaveSettings();
			break;
		}
	}

	return FALSE;
}

// GCN Hardware
static INT_PTR CALLBACK HardwareSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto file = std::wstring();
	switch (message)
	{
		case WM_INITDIALOG:
		{
			hChildDlg[1] = hDlg;
			LoadSettings(1);
			return true;
		}
		case WM_NOTIFY:
		{
			if (((NMHDR FAR*)lParam)->code == PSN_APPLY) SaveSettings();
			break;
		}
		case WM_COMMAND:
		{
			switch (wParam)
			{
				case IDC_CHOOSE_BOOTROM:
				{
					file = UI::FileOpenDialog(UI::FileType::All);
					if (!file.empty())
					{
						SetDlgItemText(hDlg, IDC_BOOTROM_FILE, file.c_str());
					}
					else
					{
						SetDlgItemText(hDlg, IDC_BOOTROM_FILE, L"");
					}

					break;
				}
				case IDC_CHOOSE_DSPDROM:
				{
					file = UI::FileOpenDialog(UI::FileType::All);
					if (!file.empty())
					{
						SetDlgItemText(hDlg, IDC_DSPDROM_FILE, file.c_str());
					}
					else
					{
						SetDlgItemText(hDlg, IDC_DSPDROM_FILE, L"");
					}

					break;
				}
				case IDC_CHOOSE_DSPIROM:
				{
					file = UI::FileOpenDialog(UI::FileType::All);
					if (!file.empty())
					{
						SetDlgItemText(hDlg, IDC_DSPIROM_FILE, file.c_str());
					}
					else
					{
						SetDlgItemText(hDlg, IDC_DSPIROM_FILE, L"");
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

void OpenSettingsDialog(HWND hParent, HINSTANCE hInst)
{
	hParentWnd = hParent;
	hParentInst = hInst;

	PROPSHEETPAGE psp[2] = { 0 };
	PROPSHEETHEADER psh = { 0 };

	// UserMenu page
	psp[0].dwSize = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags = PSP_USETITLE;
	psp[0].hInstance = hParentInst;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_GUI);
	psp[0].pfnDlgProc = UserMenuSettingsProc;
	psp[0].pszTitle = tabs[0];
	psp[0].lParam = 0;
	psp[0].pfnCallback = NULL;
	settingsLoaded[0] = FALSE;

	// Hardware page
	psp[1].dwSize = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags = PSP_USETITLE;
	psp[1].hInstance = hParentInst;
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_HW);
	psp[1].pfnDlgProc = HardwareSettingsProc;
	psp[1].pszTitle = tabs[1];
	psp[1].lParam = 0;
	psp[1].pfnCallback = NULL;
	settingsLoaded[1] = FALSE;

	// property sheet
	auto title = fmt::format(L"Configure {:s}", APPNAME);
	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_USEHICON | /*PSH_PROPTITLE |*/ PSH_NOAPPLYNOW | PSH_PROPSHEETPAGE;
	psh.hwndParent = hParentWnd;
	psh.hInstance = hParentInst;
	psh.hIcon = LoadIcon(hParentInst, MAKEINTRESOURCE(IDI_PUREI_ICON));
	psh.pszCaption = title.data();
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
	psh.nStartPage = 0;
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;
	psh.pfnCallback = NULL;

	PropertySheet(&psh);    // blocking call
}



/* ---------------------------------------------------------------------------
	Misc config section.
--------------------------------------------------------------------------- */


// ---------------------------------------------------------------------------
// file filter dialog

static void filter_string(HWND hDlg, uint32_t filter)
{
	wchar_t buf[64] = { 0 }, * ptr = buf;
	const wchar_t* mask[] = { L"*.dol", L"*.elf", L"*.gcm", L"*.iso" };

	filter = _byteswap_ulong(filter);

	for (int i = 0; i < 4; i++)
	{
		if (filter & 0xff) ptr += swprintf_s(ptr, _countof(buf) - (ptr - buf), L"%s;", mask[i]);
		filter >>= 8;
	}

	SetDlgItemText(hDlg, IDC_FILE_FILTER, buf);
}

static void check_filter(HWND hDlg, uint32_t filter)
{
	// DOL
	if (filter & 0xff000000) CheckDlgButton(hDlg, IDC_DOL_FILTER, BST_CHECKED);
	else CheckDlgButton(hDlg, IDC_DOL_FILTER, BST_UNCHECKED);
	// ELF
	if (filter & 0x00ff0000) CheckDlgButton(hDlg, IDC_ELF_FILTER, BST_CHECKED);
	else CheckDlgButton(hDlg, IDC_ELF_FILTER, BST_UNCHECKED);
	// GCM
	if (filter & 0x0000ff00) CheckDlgButton(hDlg, IDC_GCM_FILTER, BST_CHECKED);
	else CheckDlgButton(hDlg, IDC_GCM_FILTER, BST_UNCHECKED);
	// ISO
	if (filter & 0x000000ff) CheckDlgButton(hDlg, IDC_GMP_FILTER, BST_CHECKED);
	else CheckDlgButton(hDlg, IDC_GMP_FILTER, BST_UNCHECKED);
}

// dialog procedure
static INT_PTR CALLBACK FileFilterProc(
	HWND    hwndDlg,    // handle to dialog box
	UINT    uMsg,       // message
	WPARAM  wParam,     // first message parameter
	LPARAM  lParam      // second message parameter
)
{
	switch (uMsg)
	{
		// prepare dialog
		case WM_INITDIALOG:
		{
			CenterChildWindow(GetParent(hwndDlg), hwndDlg);

			// set dialog appearance
			ShowWindow(hwndDlg, SW_NORMAL);
			SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PUREI_ICON)));

			// fill by default info
			usel.filter = UI::Jdi->GetConfigInt(USER_FILTER, USER_UI);
			filter_string(hwndDlg, usel.filter);
			check_filter(hwndDlg, usel.filter);
			return TRUE;
		}

		// close button
		case WM_CLOSE:
		{
			EndDialog(hwndDlg, 0);
			return TRUE;
		}

		// dialog controls
		case WM_COMMAND:
		{
			if (wParam == IDOK)
			{
				EndDialog(hwndDlg, 0);

				// save information and update selector (if filter was changed)
				if ((uint32_t)UI::Jdi->GetConfigInt(USER_FILTER, USER_UI) != usel.filter)
				{
					UI::Jdi->SetConfigInt(USER_FILTER, usel.filter, USER_UI);
					UpdateSelector();
				}
				return TRUE;
			}
			switch (LOWORD(wParam))
			{
			case IDC_DOL_FILTER:                    // DOL
				usel.filter ^= 0xff000000;
				filter_string(hwndDlg, usel.filter);
				check_filter(hwndDlg, usel.filter);
				return TRUE;
			case IDC_ELF_FILTER:                    // ELF
				usel.filter ^= 0x00ff0000;
				filter_string(hwndDlg, usel.filter);
				check_filter(hwndDlg, usel.filter);
				return TRUE;
			case IDC_GCM_FILTER:                    // GCM
				usel.filter ^= 0x0000ff00;
				filter_string(hwndDlg, usel.filter);
				check_filter(hwndDlg, usel.filter);
				return TRUE;
			case IDC_GMP_FILTER:                    // ISO
				usel.filter ^= 0x000000ff;
				filter_string(hwndDlg, usel.filter);
				check_filter(hwndDlg, usel.filter);
				return TRUE;
			}
		}
	}

	return FALSE;
}

void EditFileFilter(HWND hwnd)
{
	DialogBox(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDD_FILE_FILTER),
		hwnd,
		FileFilterProc);
}


/* Main window controls and creation.                                       */
/* main window consist from 3 parts : main menu, selector and statusbar.    */
/* selector is active only when emulator is in Idle state (not running);    */
/* statusbar is used to show current emulator state and performance.        */
/* last note : DO NOT USE WINDOWS API CODE IN OTHER SUB-SYSTEMS!!           */

/* All important data is placed here */
UserWindow wnd;

/* ---------------------------------------------------------------------------  */
/* Statusbar                                                                    */

/* Set default values of statusbar parts */
static void ResetStatusBar()
{
	SetStatusText(STATUS_ENUM::Progress,    L"Idle");
	SetStatusText(STATUS_ENUM::VIs,         L"");
	SetStatusText(STATUS_ENUM::PEs,         L"");
	SetStatusText(STATUS_ENUM::SystemTime,  L"");
}

/* Create status bar window */
static void CreateStatusBar()
{
	int parts[] = { 0, 360, 420, 480, -1 };

	if (wnd.hMainWindow == NULL) return;

	/* Create window */
	wnd.hStatusWindow = CreateStatusWindow(
		WS_CHILD | WS_VISIBLE,
		NULL,
		wnd.hMainWindow,
		ID_STATUS_BAR
	);
	assert(wnd.hStatusWindow);

	/* Depart statusbar */
	SendMessage( wnd.hStatusWindow, 
				 SB_SETPARTS, 
				 (WPARAM)sizeof(parts) / sizeof(int), 
				 (LPARAM)parts);

	/* Set default values */
	ResetStatusBar();
}

/* Change text in specified statusbar part */
void SetStatusText(STATUS_ENUM sbPart, const std::wstring & text, bool post)
{
	if (wnd.hStatusWindow == NULL)
	{
		return;
	}

	if(post)
	{
		PostMessage(wnd.hStatusWindow, SB_SETTEXT, (WPARAM)(sbPart), (LPARAM)text.data());
	}
	else
	{
		SendMessage(wnd.hStatusWindow, SB_SETTEXT, (WPARAM)(sbPart), (LPARAM)text.data());
	}
}

/* Get text of statusbar part */
std::wstring GetStatusText(STATUS_ENUM sbPart)
{
	static auto sbText = std::wstring(256, 0);

	if (wnd.hStatusWindow == NULL) return L"";

	SendMessage(wnd.hStatusWindow, SB_GETTEXT, (WPARAM)(sbPart), (LPARAM)sbText.data());
	return sbText;
}

void StartProgress(int range, int delta)
{
	StopProgress();

	RECT rect;
	SendMessage(wnd.hStatusWindow, SB_GETRECT, 1, (LPARAM)&rect);
	int cyVScroll = GetSystemMetrics(SM_CYVSCROLL);

	wnd.hProgress = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE, 
		rect.left, rect.top + abs(rect.bottom - rect.top) / 2 - cyVScroll / 2, rect.right, cyVScroll, 
		wnd.hStatusWindow, NULL, GetModuleHandle(NULL), 0);
	if(wnd.hProgress == NULL) return;
	
	SendMessage(wnd.hProgress, PBM_SETPOS, 0, 0);
	SendMessage(wnd.hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, range));
	SendMessage(wnd.hProgress, PBM_SETSTEP, delta, 0);
}

void StepProgress()
{
	if(wnd.hProgress)
	{
		SendMessage(wnd.hProgress, PBM_STEPIT, 0, 0);
	}
}

void StopProgress()
{
	if(wnd.hProgress)
	{
		DestroyWindow(wnd.hProgress);
		wnd.hProgress = NULL;
	}
}

#pragma region "Recent files list"

#define MAX_RECENT  5   // if you want to increase, you must also add new ID_FILE_RECENT_*

/* Returns -1 if not found */
static int GetMenuItemIndex(HMENU hMenu, const std::wstring & item)
{
	int index = 0;
	wchar_t buf[MAX_PATH];

	while (index < GetMenuItemCount(hMenu))
	{
		if (GetMenuString(hMenu, index, buf, sizeof(buf) - 1, MF_BYPOSITION))
		{
			if (!wcscmp(item.c_str(), buf)) return index;
		}
		index++;
	}
	return -1;
}

static void SetRecentEntry(int index, const wchar_t* str)
{
	char var[256] = { 0, };
	sprintf_s (var, sizeof(var), USER_RECENT, index);
	UI::Jdi->SetConfigString(var, Util::WstringToString(str), USER_UI);
}

static std::wstring GetRecentEntry(int index)
{
	char var[256];
	sprintf_s (var, sizeof(var), USER_RECENT, index);
	return Util::StringToWstring(UI::Jdi->GetConfigString(var, USER_UI));
}

void UpdateRecentMenu(HWND hwnd)
{
	HMENU   hMainMenu;
	HMENU   hFileMenu;
	HMENU   hReloadMenu;
	int     idx;

	// search for required menu sub-item
	hMainMenu = GetMenu(hwnd);
	if(hMainMenu == NULL) return;

	idx = GetMenuItemIndex(hMainMenu, L"&File");             // take care about it
	if(idx < 0) return;
	hFileMenu = GetSubMenu(hMainMenu, idx);
	if(hFileMenu == NULL) return;

	idx = GetMenuItemIndex(hFileMenu, L"&Reopen\tCtrl+R");   // take care about it
	if(idx < 0) return;
	hReloadMenu = GetSubMenu(hFileMenu, idx);
	if(hReloadMenu == NULL) return;

	// clear recent list
	while(GetMenuItemCount(hReloadMenu))
	{
		DeleteMenu(hReloadMenu, 0, MF_BYPOSITION);
	}

	// if no recent, add empty
	if(UI::Jdi->GetConfigInt(USER_RECENT_NUM, USER_UI) == 0)
	{
		AppendMenu(hReloadMenu, MF_GRAYED | MF_STRING, ID_FILE_RECENT_1, L"None");
	}
	else
	{
		auto buffer = std::wstring();
		int RecentNum = UI::Jdi->GetConfigInt(USER_RECENT_NUM, USER_UI);

		for(int i = 0, n = RecentNum; i < RecentNum; i++, n--)
		{
			buffer = fmt::format(L"{:s}", UI::FileShortName(GetRecentEntry(n), 3));
			AppendMenu(hReloadMenu, MF_STRING, ID_FILE_RECENT_1 + i, buffer.data());
		}
	}

	DrawMenuBar(hwnd);
}

void AddRecentFile(const std::wstring& path)
{
	int n;
	int RecentNum = UI::Jdi->GetConfigInt(USER_RECENT_NUM, USER_UI);

	// check if item already present in list
	for (n = 1; n <= RecentNum; n++)
	{
		if (!_wcsicmp(path.c_str(), GetRecentEntry(n).c_str()))
		{
			// place old recent to the top
			// and move upper recents down
			wchar_t old[MAX_PATH] = { 0, };
			swprintf_s(old, _countof(old) - 1, L"%s", GetRecentEntry(n).c_str());
			for (n = n + 1; n <= RecentNum; n++)
			{
				SetRecentEntry(n - 1, GetRecentEntry(n).c_str());
			}
			SetRecentEntry(RecentNum, old);
			UpdateRecentMenu(wnd.hMainWindow);
			return;
		}
	}

	// increase amount of recent files
	RecentNum++;
	if (RecentNum > MAX_RECENT)
	{
		// move list up
		for (n = 1; n < MAX_RECENT; n++)
		{
			SetRecentEntry(n, GetRecentEntry(n + 1).c_str());
		}
		RecentNum = 5;
	}
	UI::Jdi->SetConfigInt(USER_RECENT_NUM, RecentNum, USER_UI);

	// add new entry
	SetRecentEntry(RecentNum, path.c_str());
	UpdateRecentMenu(wnd.hMainWindow);
}

// index = 1..max
void LoadRecentFile(int index)
{
	int RecentNum = UI::Jdi->GetConfigInt(USER_RECENT_NUM, USER_UI);
	std::wstring path = GetRecentEntry((RecentNum+1) - index);
	UI::Jdi->Unload();
	UI::Jdi->LoadFile(Util::WstringToString(path));
	if (Debug::debugger)
	{
		Debug::debugger->InvalidateAll();
	}
	OnMainWindowOpened(path.c_str());
	UI::Jdi->Run();
}

#pragma endregion "Recent files list"

// ---------------------------------------------------------------------------
// window actions, on emu start/stop

// select sort method (Options->View->Sort by)
static void SelectSort()
{
	CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_1, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_2, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_3, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_4, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_5, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_6, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_7, MF_BYCOMMAND | MF_UNCHECKED);
	
	switch((SELECTOR_SORT)UI::Jdi->GetConfigInt(USER_SORTVIEW, USER_UI))
	{
		case SELECTOR_SORT::Default:
			CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_1, MF_BYCOMMAND | MF_CHECKED);
			break;
		case SELECTOR_SORT::Filename:
			CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_2, MF_BYCOMMAND | MF_CHECKED);
			break;
		case SELECTOR_SORT::Title:
			CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_3, MF_BYCOMMAND | MF_CHECKED);
			break;
		case SELECTOR_SORT::Size:
			CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_4, MF_BYCOMMAND | MF_CHECKED);
			break;
		case SELECTOR_SORT::ID:
			CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_5, MF_BYCOMMAND | MF_CHECKED);
			break;
		case SELECTOR_SORT::Comment:
			CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_6, MF_BYCOMMAND | MF_CHECKED);
			break;
		default:
			CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_7, MF_BYCOMMAND | MF_CHECKED);
	}
}

// change Swap Controls
void ModifySwapControls(bool stateOpened)
{
	if(stateOpened)       // opened
	{
		SetMenuItemText(wnd.hMainMenu, ID_FILE_COVER, L"&Close Cover");
		EnableMenuItem(wnd.hMainMenu, ID_FILE_CHANGEDVD, MF_BYCOMMAND | MF_ENABLED);
	}
	else            // closed
	{
		SetMenuItemText(wnd.hMainMenu, ID_FILE_COVER, L"&Open Cover");
		EnableMenuItem(wnd.hMainMenu, ID_FILE_CHANGEDVD, MF_BYCOMMAND | MF_GRAYED);
	}
}

// set menu selector-related controls state
void ModifySelectorControls(bool active)
{
	if(active)
	{
		SetMenuItemText(wnd.hMainMenu, ID_OPTIONS_VIEW_DISABLE, L"&Disable Selector");
		EnableMenuItem(wnd.hMainMenu, ID_FILE_EDITINFO, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(wnd.hMainMenu, ID_FILE_REFRESH, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_ENABLED);
		EnableMenuItem( GetSubMenu(GetSubMenu(wnd.hMainMenu, 2), 1),    // Sort By..
						5, MF_BYPOSITION | MF_ENABLED);
		EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_FILEFILTER, MF_BYCOMMAND | MF_ENABLED);
	}
	else
	{
		SetMenuItemText(wnd.hMainMenu, ID_OPTIONS_VIEW_DISABLE, L"&Enable Selector");
		EnableMenuItem(wnd.hMainMenu, ID_FILE_EDITINFO, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(wnd.hMainMenu, ID_FILE_REFRESH, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem( GetSubMenu(GetSubMenu(wnd.hMainMenu, 2), 1),    // Sort By..
						5, MF_BYPOSITION | MF_GRAYED);
		EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_FILEFILTER, MF_BYCOMMAND | MF_GRAYED);
	}
}

// called once, during main window creation
static void OnMainWindowCreate(HWND hwnd)
{
	// save handlers
	wnd.hMainWindow = hwnd;
	wnd.hMainMenu = GetMenu(wnd.hMainWindow);

	// run once ?
	if (UI::Jdi->GetConfigBool(USER_RUNONCE, USER_UI))
	{
		CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_CHECKED);
	}
	else
	{
		CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_UNCHECKED);
	}

	// debugger enabled ?
	CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_UNCHECKED);
	if (UI::Jdi->GetConfigBool(USER_DOLDEBUG, USER_UI))
	{
		Debug::debugger = new Debug::Debugger();
		CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_CHECKED);
	}

	// load accelerators
	InitCommonControls();

	// always on top (not in debug)
	wnd.ontop = UI::Jdi->GetConfigBool(USER_ONTOP, USER_UI);
	if(wnd.ontop) CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, MF_CHECKED);
	else CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, MF_UNCHECKED);
	SetAlwaysOnTop(wnd.hMainWindow, wnd.ontop);

	// recent menu
	UpdateRecentMenu(wnd.hMainWindow);

	// dvd swap controls
	ModifySwapControls(false);

	// child windows
	CreateStatusBar();
	ResizeMainWindow(640, 480-32);  // 32 pixels overscan

	// selector disabled ?
	usel.active = UI::Jdi->GetConfigBool(USER_SELECTOR, USER_UI);
	ModifySelectorControls(usel.active);

	// icon size
	bool smallSize = UI::Jdi->GetConfigBool(USER_SMALLICONS, USER_UI);
	if(smallSize)
	{
		CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_UNCHECKED);
		CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_CHECKED);
	}
	else
	{
		CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_UNCHECKED);
		CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_CHECKED);
	}

	// select sort method
	SelectSort();

	// enable drop operation
	DragAcceptFiles(wnd.hMainWindow, TRUE);

	// Add UI methods
	JdiAddNode(UI_JDI_JSON, UIReflector);
	JdiAddNode(DEBUG_UI_JDI_JSON, Debug::DebugUIReflector);

	// simulate close operation, like we just stopped emu
	OnMainWindowClosed();
}

// called once, when emu exits to OS
static void OnMainWindowDestroy()
{
	UI::Jdi->Unload();

	JdiRemoveNode(UI_JDI_JSON);
	JdiRemoveNode(DEBUG_UI_JDI_JSON);

	// disable drop operation
	DragAcceptFiles(wnd.hMainWindow, FALSE);

	if (Debug::debugger)
	{
		delete Debug::debugger;
	}

	UI::Jdi->ExecuteCommand("exit");
}

// emulation has started - do proper actions
void OnMainWindowOpened(const wchar_t* currentFileName)
{
	// disable selector
	CloseSelector();
	ModifySelectorControls(false);
	EnableMenuItem( GetSubMenu(wnd.hMainMenu, GetMenuItemIndex(     // View
					wnd.hMainMenu, L"&Options") ), 1, MF_BYPOSITION | MF_GRAYED );

	std::wstring newTitle, gameTitle;
	wchar_t drive[_MAX_DRIVE + 1] = { 0, }, dir[_MAX_DIR] = { 0, }, name[_MAX_PATH] = { 0, }, ext[_MAX_EXT] = { 0, };
	bool dvd = false;
	bool bootrom = !wcscmp(currentFileName, L"Bootrom");

	if (!bootrom)
	{
		wchar_t* extension = wcsrchr((wchar_t*)currentFileName, L'.');

		if (!_wcsicmp(extension, L".dol"))
		{
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".elf"))
		{
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".iso"))
		{
			dvd = true;
		}
		else if (!_wcsicmp(extension, L".gcm"))
		{
			dvd = true;
		}

		_wsplitpath_s(currentFileName,
			drive, _countof(drive) - 1,
			dir, _countof(dir) - 1,
			name, _countof(name) - 1,
			ext, _countof(ext) - 1);
	}

	// set new title for main window

	if (dvd)
	{
		UI::Jdi->DvdMount(Util::WstringToString(currentFileName));

		// get DiskID
		std::vector<uint8_t> diskID;
		diskID.resize(4);
		UI::Jdi->DvdSeek(0);
		UI::Jdi->DvdRead(diskID);

		// Get title from banner

		std::vector<uint8_t> bnrRaw = DVDLoadBanner(currentFileName);

		DVDBanner2* bnr = (DVDBanner2*)bnrRaw.data();

		wchar_t longTitle[0x200];

		char* ansiPtr = (char*)bnr->comments[0].longTitle;
		wchar_t* wcharPtr = longTitle;

		while (*ansiPtr)
		{
			*wcharPtr++ = (uint8_t)*ansiPtr++;
		}
		*wcharPtr++ = 0;

		// Convert SJIS Title to Unicode

		if ( UI::Jdi->DvdRegionById((char *)diskID.data()) == "JPN")
		{
			size_t size, chars;
			uint16_t* widePtr = SjisToUnicode(longTitle, &size, &chars);
			uint16_t* unicodePtr;

			if (widePtr)
			{
				wcharPtr = longTitle;
				unicodePtr = widePtr;

				while (*unicodePtr)
				{
					*wcharPtr++ = *unicodePtr++;
				}
				*wcharPtr++ = 0;

				free(widePtr);
			}
		}

		// Update recent files list and add selector path

		wchar_t fullPath[MAX_PATH];

		swprintf_s(fullPath, _countof(fullPath) - 1, L"%s%s", drive, dir);

		// add new recent entry
		//AddRecentFile(currentFileName);

		// add new path to selector
		AddSelectorPath(fullPath);      // all checks are there

		gameTitle = longTitle;
		newTitle = fmt::format(L"{:s} - Running {:s}", APPNAME, gameTitle);
	}
	else
	{
		if (bootrom)
		{
			gameTitle = currentFileName;
		}
		else
		{
			gameTitle = fmt::format(L"{:s} demo", name);
		}
		
		newTitle = fmt::format(L"{:s} - Running {:s}", APPNAME, gameTitle);
	}
	
	SetWindowText(wnd.hMainWindow, newTitle.c_str());

	UI::g_perfMetrics = new UI::PerfMetrics();
}

// emulation stop in progress
void OnMainWindowClosed()
{
	delete UI::g_perfMetrics;

	// restore current working directory
	SetCurrentDirectory(wnd.cwd.c_str());

	// enable selector
	CreateSelector();
	ModifySelectorControls(usel.active);
	EnableMenuItem(GetSubMenu(wnd.hMainMenu, GetMenuItemIndex(     // View
		wnd.hMainMenu, L"&Options")), 1, MF_BYPOSITION | MF_ENABLED);

	// set to Idle
	auto win_name = fmt::format(L"{:s} - {:s} ({:s})", APPNAME, APPDESC, Util::StringToWstring(UI::Jdi->GetVersion()));
	SetWindowText(wnd.hMainWindow, win_name.c_str());
	ResetStatusBar();
}

// ---------------------------------------------------------------------------
// window controls

// resize client area to fit in given width and height
void ResizeMainWindow(int width, int height)
{
	RECT rc;
	int x, y, w, h;

	GetWindowRect(wnd.hMainWindow, &rc);

	// left-upper corner
	x = rc.left;
	y = rc.top;

	// calculate adjustment
	rc.left = 0;
	rc.top = 0;
	rc.right = width;
	rc.bottom = height;
	AdjustWindowRect(&rc, WIN_STYLE, 1);

	// width and height
	w = rc.right - rc.left;
	h = rc.bottom - rc.top + GetSystemMetrics(SM_CYCAPTION) + 9;

	// adjust by statusbar height
	if(IsWindow(wnd.hStatusWindow))
	{
		GetWindowRect(wnd.hStatusWindow, &rc);
		h += (WORD)(rc.bottom - rc.top);
	}

	// move window
	MoveWindow(wnd.hMainWindow, x, y, w, h, TRUE);
	SendMessage(wnd.hMainWindow, WM_SIZE, 0, 0);
}

/* Main window procedure : "return 0" to leave, "break" to continue DefWindowProc() */
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto name = std::wstring();
	int recent;

	switch(msg)
	{
		/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */ 
		/* Create/destroy messages                                                      */

		case WM_CREATE:
		{
			OnMainWindowCreate(hwnd);
			return 0;
		}
		case WM_CLOSE:
		{
			DestroyWindow(hwnd);
			return 0;
		}
		case WM_DESTROY:
		{
			OnMainWindowDestroy();
			EMUDtor();
			return 0;
		}
		
		/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
		/* Window controls                                                              */

		case WM_COMMAND:
		{
			/* Load recent file */
			if (LOWORD(wParam) >= ID_FILE_RECENT_1 &&
				LOWORD(wParam) <= ID_FILE_RECENT_5)
			{
				LoadRecentFile(LOWORD(wParam) - ID_FILE_RECENT_1 + 1);
			}
			else switch (LOWORD(wParam))
			{
				/* Load DVD/executable (START) */
				case ID_FILE_LOAD:
				{
					name = UI::FileOpenDialog(UI::FileType::All);
					if (!name.empty())
					{
					loadFile:
					
						UI::Jdi->LoadFile(Util::WstringToString(name));
						if (Debug::debugger)
						{
							Debug::debugger->InvalidateAll();
						}
						OnMainWindowOpened(name.c_str());
						UI::Jdi->Run();
					}

					return 0;
				}
				/* Reload last opened file (RESET) */
				case ID_FILE_RELOAD:
				{
					recent = UI::Jdi->GetConfigInt(USER_RECENT_NUM, USER_UI);
					if (recent > 0)
					{
						LoadRecentFile(1);
					}

					return 0;
				}
				/* Unload file (STOP) */
				case ID_FILE_UNLOAD:
				{
					UI::Jdi->Stop();
					Sleep(100);
					UI::Jdi->Unload();
					OnMainWindowClosed();

					return 0;
				}
				/* Load bootrom */
				case ID_FILE_IPLMENU:
				{
					UI::Jdi->LoadFile("Bootrom");
					OnMainWindowOpened(L"Bootrom");
					if (Debug::debugger == nullptr)
					{
						UI::Jdi->Run();
					}
					else
					{
						Debug::debugger->SetDisasmCursor(0xfff0'0100);
						UI::Jdi->ExecuteCommand("echo \"Bootrom is started in Suspended state for debugging purposes. Press F5 to continue.\"");
					}
					return 0;
				}
				/* Open/close DVD lid */
				case ID_FILE_COVER:
				{
					if ( UI::Jdi->DvdCoverOpened() )   /* Close lid */
					{
						UI::Jdi->DvdCloseCover();
						ModifySwapControls(false);
					}
					else /* Open lid */
					{
						UI::Jdi->DvdOpenCover();
						ModifySwapControls(true);
					}

					return 0;
				}
				/* Set new current DVD image. */
				case ID_FILE_CHANGEDVD:
				{
					name = UI::FileOpenDialog(UI::FileType::Dvd);
					if (!name.empty() && UI::Jdi->DvdCoverOpened() )
					{
						/* Bad */
						if (! UI::Jdi->DvdMount ( Util::WstringToString(name)) )
						{
							return 0;
						}

						/* Close lid */
						UI::Jdi->DvdCloseCover();
						ModifySwapControls(false);
					}

					return 0;
				}
				/* Exit to OS */
				case ID_FILE_EXIT:
				{
					DestroyWindow(hwnd);
					return 0;
				}
				/* Settings dialog */
				case ID_OPTIONS_SETTINGS:
				{
					OpenSettingsDialog(hwnd, GetModuleHandle(NULL));
					return 0;
				}
				/* Always on top */
				case ID_OPTIONS_ALWAYSONTOP:
				{
					wnd.ontop = wnd.ontop ? false : true;
					UI::Jdi->SetConfigBool(USER_ONTOP, wnd.ontop, USER_UI);

					auto flags = (wnd.ontop ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND | MF_UNCHECKED);
					CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, flags);

					SetAlwaysOnTop(hwnd, wnd.ontop);
					return 0;
				}
				/* Refresh view */
				case ID_FILE_REFRESH:
				{
					UpdateSelector();
					return 0;
				}
				/* Disable the view */
				case ID_OPTIONS_VIEW_DISABLE:
				{
					if (usel.active)
					{
						CloseSelector();
						ResetStatusBar();
						ModifySelectorControls(false);
						usel.active = false;
						UI::Jdi->SetConfigBool(USER_SELECTOR, false, USER_UI);
					}
					else
					{
						ModifySelectorControls(true);
						usel.active = true;
						UI::Jdi->SetConfigBool(USER_SELECTOR, true, USER_UI);
						CreateSelector();
					}

					return 0;
				}
				/* Change icon size */
				case ID_OPTIONS_VIEW_SMALLICONS:
				{
					CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_UNCHECKED);
					CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_CHECKED);
					SetSelectorIconSize(TRUE);
					return 0;
				}
				case ID_OPTIONS_VIEW_LARGEICONS:
				{
					CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_UNCHECKED);
					CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_CHECKED);
					SetSelectorIconSize(FALSE);
					return 0;
				}
				/* Sort files */
				case ID_OPTIONS_VIEW_SORTBY_1:
				{
					SortSelector(SELECTOR_SORT::Default);
					SelectSort();

					return 0;
				}
				case ID_OPTIONS_VIEW_SORTBY_2:
				{
					SortSelector(SELECTOR_SORT::Filename);
					SelectSort();

					return 0;
				}
				case ID_OPTIONS_VIEW_SORTBY_3:
				{
					SortSelector(SELECTOR_SORT::Title);
					SelectSort();

					return 0;
				}
				case ID_OPTIONS_VIEW_SORTBY_4:
				{
					SortSelector(SELECTOR_SORT::Size);
					SelectSort();

					return 0;
				}
				case ID_OPTIONS_VIEW_SORTBY_5:
				{
					SortSelector(SELECTOR_SORT::ID);
					SelectSort();

					return 0;
				}
				case ID_OPTIONS_VIEW_SORTBY_6:
				{
					SortSelector(SELECTOR_SORT::Comment);
					SelectSort();

					return 0;
				}
				case ID_OPTIONS_VIEW_SORTBY_7:
				{
					SortSelector(SELECTOR_SORT::Unsorted);
					SelectSort();

					return 0;
				}

				/* Edit file filter */
				case ID_OPTIONS_VIEW_FILEFILTER:
				{
					EditFileFilter(hwnd);
					return 0;
				}

				/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
				/* debug controls                                                               */

				// multiple instancies on/off
				case ID_RUN_ONCE:
				{
					if (UI::Jdi->GetConfigBool(USER_RUNONCE, USER_UI))
					{   /* Off */
						CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_UNCHECKED);
						UI::Jdi->SetConfigBool(USER_RUNONCE, false, USER_UI);
					}
					else
					{   /* On */
						CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_CHECKED);
						UI::Jdi->SetConfigBool(USER_RUNONCE, true, USER_UI);
					}

					return 0;
				}
				// Open/close system-wide debugger
				case ID_DEBUG_CONSOLE:
				{
					if (Debug::debugger == nullptr)
					{   // open
						CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_CHECKED);
						Debug::debugger = new Debug::Debugger();
						UI::Jdi->SetConfigBool(USER_DOLDEBUG, true, USER_UI);
						SetStatusText(STATUS_ENUM::Progress, L"Debugger opened");
					}
					else
					{   // close
						CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_UNCHECKED);
						delete Debug::debugger;
						Debug::debugger = nullptr;
						UI::Jdi->SetConfigBool(USER_DOLDEBUG, false, USER_UI);
						SetStatusText(STATUS_ENUM::Progress, L"Debugger closed");
					}
					return 0;
				}
				// Mount Dolphin SDK as DVD
				case ID_DEVELOPMENT_MOUNTSDK:
				{
					std::wstring dolphinSdkDir = UI::FileOpenDialog(UI::FileType::Directory);
					if (!dolphinSdkDir.empty())
					{
						UI::Jdi->DvdMountSDK(Util::WstringToString(dolphinSdkDir));
					}

					return 0;
				}

				// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
				// options dialogs

				case ID_OPTIONS_CONTROLLERS_PORT1:
					PADConfigure(0, wnd.hMainWindow);
					return 0;
				case ID_OPTIONS_CONTROLLERS_PORT2:
					PADConfigure(1, wnd.hMainWindow);
					return 0;
				case ID_OPTIONS_CONTROLLERS_PORT3:
					PADConfigure(2, wnd.hMainWindow);
					return 0;
				case ID_OPTIONS_CONTROLLERS_PORT4:
					PADConfigure(3, wnd.hMainWindow);
					return 0;

				// configure memcard in slot A
				case ID_OPTIONS_MEMCARDS_SLOTA:
					MemcardConfigure(0, hwnd);
					return 0;

				// configure memcard in slot B
				case ID_OPTIONS_MEMCARDS_SLOTB:
					MemcardConfigure(1, hwnd);
					return 0;

				case ID_HELP_ABOUT:
					AboutDialog(hwnd);
					return 0;
				}
				break;
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
		// file drag & drop operation

		case WM_DROPFILES:
		{
			wchar_t fileName[MAX_PATH] = { 0 };
			DragQueryFile((HDROP)wParam, 0, fileName, sizeof(fileName));
			DragFinish((HDROP)wParam);

			// extension filter
			if(_wcsicmp(L".dol", wcsrchr(fileName, L'.')) &&
			   _wcsicmp(L".elf", wcsrchr(fileName, L'.')) &&
			   _wcsicmp(L".iso", wcsrchr(fileName, L'.')) &&
			   _wcsicmp(L".gcm", wcsrchr(fileName, L'.')) ) break;

			name = fileName;
			goto loadFile;
		}
		return 0;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
		// resizing of main window

		case WM_SIZE:
		{
			// resize status bar window
			if(IsWindow(wnd.hStatusWindow))
			{
				RECT rm, rs;
				GetWindowRect(hwnd, &rm);
				GetWindowRect(wnd.hStatusWindow, &rs); 
				long sbh = (WORD)(rs.bottom - rs.top);
				MoveWindow(wnd.hStatusWindow, rm.left, rm.top, rm.right - rm.left, rm.bottom - sbh, TRUE);
			}

			// resize selector
			{
				RECT rc;
				GetClientRect(hwnd, &rc);
				ResizeSelector(
					(WORD)(rc.right - rc.left),
					(WORD)(rc.bottom - rc.top)
				);
			}

			// Resize xfb renderer
			VideoOutResize(LOWORD(lParam), HIWORD(lParam));

			// Resize gfx renderer
			Flipper::Gx->ResizeRenderTarget(LOWORD(lParam), HIWORD(lParam));

			return 0;
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
		// selector notification messages

		case WM_NOTIFY:
		{
			if(wParam == ID_SELECTOR)
			{
				NotifySelector((LPNMHDR)lParam);

				// sort rules may change, by clicking on column
				SelectSort();
			}
			return 0;
		}

		case WM_DRAWITEM:
		{
			if(wParam == ID_SELECTOR)
			{
				DrawSelectorItem((LPDRAWITEMSTRUCT)lParam);
			}
			return 0;
		}        
	}
	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// self-explanatory.. creates main window and all child windows.
// window size will be set to default 400x300.
HWND CreateMainWindow(HINSTANCE hInstance)
{
	WNDCLASS wc = { 0 };
	const wchar_t CLASS_NAME[] = L"GAMECUBECLASS";

	assert(wnd.hMainWindow == nullptr);

	wc.cbClsExtra    = wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PUREI_ICON));
	wc.hInstance     = hInstance;
	wc.lpfnWndProc   = WindowProc;
	wc.lpszClassName = CLASS_NAME;
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MAIN_MENU);
	wc.style         = 0;

	ATOM classAtom = RegisterClass(&wc);
	assert(classAtom != 0);

	auto win_name = fmt::format(L"{:s} - {:s} ({:s})", APPNAME, APPDESC, Util::StringToWstring(UI::Jdi->GetVersion()));
	wnd.hMainWindow = CreateWindowEx(
		0,
		CLASS_NAME,
		win_name.c_str(),
		WIN_STYLE, 
		CW_USEDEFAULT, CW_USEDEFAULT,
		400, 300,
		NULL, NULL,
		hInstance, NULL);

	assert(wnd.hMainWindow);

	ShowWindow(wnd.hMainWindow, SW_NORMAL);
	UpdateWindow(wnd.hMainWindow);

	return wnd.hMainWindow;
}

// ---------------------------------------------------------------------------
// utilities

// change main window on-top state
void SetAlwaysOnTop(HWND hwnd, BOOL state)
{
	RECT rect;
	HWND ontop[] = { HWND_NOTOPMOST, HWND_TOPMOST };
	GetWindowRect(hwnd, &rect);
	SetWindowPos(
		hwnd, 
		ontop[state], 
		rect.left, rect.top, 
		rect.right - rect.left, rect.bottom - rect.top, 
		SWP_SHOWWINDOW
	);
	UpdateWindow(hwnd);
}

// center the child window into the parent window
void CenterChildWindow(HWND hParent, HWND hChild)
{
   if (IsWindow(hParent) && IsWindow(hChild))
   {
		RECT rp, rc;

		GetWindowRect(hParent, &rp);
		GetWindowRect(hChild, &rc);

		MoveWindow(hChild, 
			rp.left + (rp.right - rp.left - rc.right + rc.left) / 2,
			rp.top + (rp.bottom - rp.top - rc.bottom + rc.top) / 2,
			rc.right - rc.left, rc.bottom - rc.top, TRUE
		);
	}
}

void SetMenuItemText(HMENU hmenu, UINT id, const std::wstring & text)
{
	MENUITEMINFO info = { 0 };
	   
	info.cbSize = sizeof(MENUITEMINFO);
	info.fMask  = MIIM_TYPE;
	info.fType  = MFT_STRING;
	info.dwTypeData = (LPTSTR)text.data();

	SetMenuItemInfo(hmenu, id, FALSE, &info);
}
