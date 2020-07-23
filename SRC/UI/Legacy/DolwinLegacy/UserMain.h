
#pragma once

// version info
#define APPNAME _T("Dolwin")
#define APPDESC _T("Nintendo Gamecube Emulator for Windows")

// Will be derived from JDI
//#define APPVER  _T("0.131")

namespace UI
{
	// basic message output
	void DolwinError(const TCHAR* title, const TCHAR* fmt, ...);
	bool DolwinQuestion(const TCHAR* title, const TCHAR* fmt, ...);
	void DolwinReport(const TCHAR* fmt, ...);
}
