#pragma once
#include <codecvt>
#include <locale>
#include <string>

std::wstring str_to_wstr(const std::string& str);
std::string wstr_to_str(const std::wstring& wstr);
