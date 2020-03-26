
#pragma once

namespace UI
{

	// basic message output
	void    DolwinError(const TCHAR* title, const TCHAR* fmt, ...);
	BOOL    DolwinQuestion(const TCHAR* title, const TCHAR* fmt, ...);
	void    DolwinReport(const TCHAR* fmt, ...);

}
