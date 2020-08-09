
#pragma once

// version info
#define APPNAME L"Dolwin"
#define APPDESC L"Nintendo Gamecube Emulator"

namespace UI
{
	// basic message output
	void DolwinError(const wchar_t* title, const wchar_t* fmt, ...);
	void DolwinReport(const wchar_t* fmt, ...);
}
