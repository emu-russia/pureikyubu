
#pragma once

namespace Debug
{

	enum class GekkoRegmode
	{
		GPR = 0,
		FPR,
		PSR,
		MMU
	};

	class GekkoRegs : public CuiWindow
	{
		GekkoRegmode mode = GekkoRegmode::GPR;

		void Memorize();

		void ShowGprs();
		void ShowFprs();
		void ShowPairedSingle();
		void ShowMmu();

		void RotateView(bool forward);

	public:
		GekkoRegs(RECT& rect, std::string name, Cui* parent);
		~GekkoRegs();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
