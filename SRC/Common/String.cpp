#include "pch.h"

namespace Util
{
	std::string WstringToString(std::wstring& wstr)
	{
		std::string str;
		str.reserve(wstr.size());
		for (auto it = wstr.begin(); it != wstr.end(); ++it)
		{
			str.push_back((char)*it);
		}
		return str;
	}

	std::wstring StringToWstring(std::string& str)
	{
		std::wstring wstr;
		wstr.reserve(str.size());
		for (auto it = str.begin(); it != str.end(); ++it)
		{
			wstr.push_back((wchar_t)*it);
		}
		return wstr;
	}

	std::string TcharToString(TCHAR* tstr)
	{
		char ansiText[0x200] = { 0, };
		char* ansiPtr = ansiText;
		TCHAR* tcharPtr = tstr;
		while (*tcharPtr)
		{
			*ansiPtr++ = (char)*tcharPtr++;
		}
		*ansiPtr++ = 0;
		return std::string(ansiText);
	}

	std::wstring TcharToWstring(TCHAR* tstr)
	{
		wchar_t wideText[0x200] = { 0, };
		wchar_t* widePtr = wideText;
		TCHAR* tcharPtr = tstr;
		while (*tcharPtr)
		{
			*widePtr++ = (wchar_t)*tcharPtr++;
		}
		*widePtr++ = 0;
		return std::wstring(wideText);
	}

}
