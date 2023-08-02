#include "pch.h"

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
}
