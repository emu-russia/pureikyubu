// System-wide debugger

#pragma once

namespace Debug
{

	class GekkoDebug : public Cui
	{
		static const size_t width = 120;
		static const size_t height = 100;

	public:
		GekkoDebug();

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
