// Gekko architecture definitions (from datasheet).

#pragma once

// GC bus clock is running on 1/3 of CPU clock, and timer is
// running on 1/4 of bus clock (1/12 of CPU clock)
#define CPU_CORE_CLOCK  486000000u  // 486 mhz (its not 485, stop bugging me!)
#define CPU_BUS_CLOCK   (CPU_CORE_CLOCK / 3)
#define CPU_TIMER_CLOCK (CPU_BUS_CLOCK / 4)

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

#define HID2_LSQE   0x80000000          // PS load/store quantization
#define HID2_WPE    0x40000000          // gathering enabled
#define HID2_PSE    0x20000000          // PS-mode
#define HID2_LCE    0x10000000          // cache is locked

#define WPAR_ADDR   0xffffffe0          // accumulation address
#define WPAR_BNE    0x1                 // buffer not empty

#define GEKKO_CR0_LT    (1 << 31)		// Result < 0
#define GEKKO_CR0_GT    (1 << 30)		// Result > 0
#define GEKKO_CR0_EQ    (1 << 29)		// Result == 0
#define GEKKO_CR0_SO    (1 << 28)		// Copied from XER[SO]

#define GEKKO_XER_SO    (1 << 31)		// Sticky overflow
#define GEKKO_XER_OV    (1 << 30)		// Overflow 
#define GEKKO_XER_CA    (1 << 29)		// Carry

// Exception vectors (physical address)

namespace Gekko
{
	enum class Exception : uint32_t
	{
		RESET = 0x0100,
		MACHINE = 0x0200,
		DSI = 0x0300,
		ISI = 0x0400,
		INTERRUPT = 0x0500,
		ALIGN = 0x0600,
		PROGRAM = 0x0700,
		FPUNAVAIL = 0x0800,
		DECREMENTER = 0x0900,
		SYSCALL = 0x0C00,
		PERFMON = 0x0D00,
		IABR = 0x1300,
		THERMAL = 0x1700,
	};
}

// Sprs

namespace Gekko
{
	enum class SPR : int
	{
		XER = 1,
		LR = 8,
		CTR = 9,
		DSISR = 18,
		DAR = 19,
		DEC = 22,
		SDR1 = 25,
		SRR0 = 26,
		SRR1 = 27,
		SPRG0 = 272,
		SPRG1 = 273,
		SPRG2 = 274,
		SPRG3 = 275,
		EAR = 282,
		PVR = 287,
		IBAT0U = 528,
		IBAT0L = 529,
		IBAT1U = 530,
		IBAT1L = 531,
		IBAT2U = 532,
		IBAT2L = 533,
		IBAT3U = 534,
		IBAT3L = 535,
		DBAT0U = 536,
		DBAT0L = 537,
		DBAT1U = 538,
		DBAT1L = 539,
		DBAT2U = 540,
		DBAT2L = 541,
		DBAT3U = 542,
		DBAT3L = 543,
		HID0 = 1008,
		HID1 = 1009,
		IABR = 1010,
		DABR = 1013,
		GQRs = 912,
		GQR0 = 912,
		GQR1 = 913,
		GQR2 = 914,
		GQR3 = 915,
		GQR4 = 916,
		GQR5 = 917,
		GQR6 = 918,
		GQR7 = 919,
		HID2 = 920,
		WPAR = 921,
		DMAU = 922,
		DMAL = 923,
	};
}
