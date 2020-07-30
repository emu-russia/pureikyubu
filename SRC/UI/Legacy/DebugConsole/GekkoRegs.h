
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

	#pragma pack(push, 1)

	union Fpreg
	{
		struct
		{
			uint32_t	Low;
			uint32_t	High;
		} u;
		uint64_t Raw;
		double Float;
	};

	#pragma pack(pop)

	class GekkoRegs : public CuiWindow
	{
		GekkoRegmode mode = GekkoRegmode::GPR;

		uint32_t savedGpr[32] = { 0 };
		Fpreg savedPs0[32] = { 0 };
		Fpreg savedPs1[32] = { 0 };

		void Memorize();

		void ShowGprs();
		void ShowOtherRegs();
		void ShowFprs();
		void ShowPairedSingle();
		void ShowMmu();

		void RotateView(bool forward);

		void print_gprreg(int x, int y, int num);
		void print_fpreg(int x, int y, int num);
		void print_ps(int x, int y, int num);
		int cntlzw(uint32_t val);
		void describe_bat_reg(int x, int y, uint32_t up, uint32_t lo, bool instr);
		std::string smart_size(size_t size);

	public:
		GekkoRegs(RECT& rect, std::string name, Cui* parent);
		~GekkoRegs();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
