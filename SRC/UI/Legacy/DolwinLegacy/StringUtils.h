#pragma once

namespace Util
{
	std::string WstringToString(const std::wstring& wstr);

	std::wstring StringToWstring(const std::string& str);

	std::string TcharToString(const TCHAR* tstr);

	std::wstring TcharToWstring(const TCHAR* tstr);
}
