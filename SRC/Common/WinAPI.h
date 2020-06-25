#pragma once

namespace Win
{
	/* Open a file dialog and return the path the user selected. */
	std::wstring OpenDialog(std::wstring_view title, std::wstring_view filer, bool pick_folder = false);
}
