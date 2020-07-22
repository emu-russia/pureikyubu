#pragma once

#include <string>

namespace Util
{
	std::string WstringToString(std::wstring& wstr);

	std::wstring StringToWstring(std::string& str);

	std::string TcharToString(const TCHAR* tstr);

	std::wstring TcharToWstring(const TCHAR* tstr);
}
