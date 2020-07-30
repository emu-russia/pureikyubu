
#pragma once

namespace Debug
{

	// Machine State Flags
	#define MSR_BIT(n)			(1 << (31-n))
	#define MSR_RESERVED        0xFFFA0088
	#define MSR_POW             (MSR_BIT(13))               // Power management enable
	#define MSR_ILE             (MSR_BIT(15))               // Exception little-endian mode
	#define MSR_EE              (MSR_BIT(16))               // External interrupt enable
	#define MSR_PR              (MSR_BIT(17))               // User privilege level
	#define MSR_FP              (MSR_BIT(18))               // Floating-point available
	#define MSR_ME              (MSR_BIT(19))               // Machine check enable
	#define MSR_FE0             (MSR_BIT(20))               // Floating-point exception mode 0
	#define MSR_SE              (MSR_BIT(21))               // Single-step trace enable
	#define MSR_BE              (MSR_BIT(22))               // Branch trace enable
	#define MSR_FE1             (MSR_BIT(23))               // Floating-point exception mode 1
	#define MSR_IP              (MSR_BIT(25))               // Exception prefix
	#define MSR_IR              (MSR_BIT(26))               // Instruction address translation
	#define MSR_DR              (MSR_BIT(27))               // Data address translation
	#define MSR_PM              (MSR_BIT(29))               // Performance monitor mode
	#define MSR_RI              (MSR_BIT(30))               // Recoverable exception
	#define MSR_LE              (MSR_BIT(31))               // Little-endian mode enable

	#define HID0_EMCP	0x8000'0000
	#define HID0_DBP	0x4000'0000
	#define HID0_EBA	0x2000'0000
	#define HID0_EBD	0x1000'0000
	#define HID0_BCLK	0x0800'0000
	#define HID0_ECLK	0x0200'0000
	#define HID0_PAR	0x0100'0000
	#define HID0_DOZE	0x0080'0000
	#define HID0_NAP	0x0040'0000
	#define HID0_SLEEP	0x0020'0000
	#define HID0_DPM	0x0010'0000
	#define HID0_NHR	0x0001'0000			// Not hard reset (software-use only)
	#define HID0_ICE	0x0000'8000
	#define HID0_DCE	0x0000'4000
	#define HID0_ILOCK	0x0000'2000
	#define HID0_DLOCK	0x0000'1000
	#define HID0_ICFI	0x0000'0800
	#define HID0_DCFI	0x0000'0400
	#define HID0_SPD	0x0000'0200
	#define HID0_IFEM	0x0000'0100
	#define HID0_SGE	0x0000'0080
	#define HID0_DCFA	0x0000'0040
	#define HID0_BTIC	0x0000'0020
	#define HID0_ABE	0x0000'0008
	#define HID0_BHT	0x0000'0004
	#define HID0_NOOPTI	0x0000'0001

	#define HID2_LSQE   0x8000'0000          // PS load/store quantization
	#define HID2_WPE    0x4000'0000          // gathering enabled
	#define HID2_PSE    0x2000'0000          // PS-mode
	#define HID2_LCE    0x1000'0000          // locked cache enable

	#define Gekko_SPR_XER 1
	#define Gekko_SPR_LR 8
	#define Gekko_SPR_CTR 9
	#define Gekko_SPR_DSISR 18
	#define Gekko_SPR_DAR 19
	#define Gekko_SPR_DEC 22
	#define Gekko_SPR_SDR1 25
	#define Gekko_SPR_SRR0 26
	#define Gekko_SPR_SRR1 27
	#define Gekko_SPR_SPRG0 272
	#define Gekko_SPR_SPRG1 273
	#define Gekko_SPR_SPRG2 274
	#define Gekko_SPR_SPRG3 275
	#define Gekko_SPR_EAR 282
	#define Gekko_SPR_TBL 284
	#define Gekko_SPR_TBU 285
	#define Gekko_SPR_PVR 287
	#define Gekko_SPR_IBAT0U 528
	#define Gekko_SPR_IBAT0L 529
	#define Gekko_SPR_IBAT1U 530
	#define Gekko_SPR_IBAT1L 531
	#define Gekko_SPR_IBAT2U 532
	#define Gekko_SPR_IBAT2L 533
	#define Gekko_SPR_IBAT3U 534
	#define Gekko_SPR_IBAT3L 535
	#define Gekko_SPR_DBAT0U 536
	#define Gekko_SPR_DBAT0L 537
	#define Gekko_SPR_DBAT1U 538
	#define Gekko_SPR_DBAT1L 539
	#define Gekko_SPR_DBAT2U 540
	#define Gekko_SPR_DBAT2L 541
	#define Gekko_SPR_DBAT3U 542
	#define Gekko_SPR_DBAT3L 543
	#define Gekko_SPR_HID0 1008
	#define Gekko_SPR_HID1 1009
	#define Gekko_SPR_IABR 1010
	#define Gekko_SPR_DABR 1013
	#define Gekko_SPR_GQRs 912
	#define Gekko_SPR_GQR0 912
	#define Gekko_SPR_GQR1 913
	#define Gekko_SPR_GQR2 914
	#define Gekko_SPR_GQR3 915
	#define Gekko_SPR_GQR4 916
	#define Gekko_SPR_GQR5 917
	#define Gekko_SPR_GQR6 918
	#define Gekko_SPR_GQR7 919
	#define Gekko_SPR_HID2 920
	#define Gekko_SPR_WPAR 921
	#define Gekko_SPR_DMAU 922
	#define Gekko_SPR_DMAL 923

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
