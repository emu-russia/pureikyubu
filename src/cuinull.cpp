// Empty CUI implementation, can be used for custom Cui implementations (e.g. imgui).
#include "pch.h"

namespace Debug
{
	// CuiWindow

	CuiWindow::CuiWindow(CuiRect& rect, std::string name, Cui* parent) {}
	CuiWindow::~CuiWindow() {}

	void CuiWindow::Print(CuiColor back, CuiColor front, int x, int y, std::string text) {}
	void CuiWindow::Print(CuiColor front, int x, int y, std::string text) {}
	void CuiWindow::Print(CuiColor back, CuiColor front, int x, int y, const char* fmt, ...) {}
	void CuiWindow::Print(CuiColor front, int x, int y, const char* fmt, ...) {}
	void CuiWindow::Fill(CuiColor back, CuiColor front, char c) {}
	void CuiWindow::FillLine(CuiColor back, CuiColor front, int y, char c) {}

	void CuiWindow::SetCursor(int x, int y) {}

	// Cui

	Cui::Cui(std::string title, size_t width, size_t height) {}
	Cui::~Cui() {}

	void Cui::AddWindow(CuiWindow* wnd) {}

	void Cui::SetWindowFocus(const std::string& name) {}

	void Cui::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl) {}

	void Cui::ShowCursor(bool show) {}
	void Cui::SetCursor(int x, int y) {}

	void Cui::InvalidateAll() {}
}
