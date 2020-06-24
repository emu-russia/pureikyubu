
#pragma once

namespace UI
{
	// basic message output
	void DolwinError(std::wstring_view title, std::wstring_view fmt, ...);
	bool DolwinQuestion(std::wstring_view title, std::wstring_view fmt, ...);
	void DolwinReport(std::wstring_view fmt, ...);
}
