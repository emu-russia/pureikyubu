
#pragma once

// version info
#define APPNAME _T("Dolwin")
#define APPDESC _T("Nintendo Gamecube Emulator")

namespace UI
{
	// basic message output
	void DolwinError(const TCHAR* title, const TCHAR* fmt, ...);
	void DolwinReport(const TCHAR* fmt, ...);
}
