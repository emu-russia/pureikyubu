// Gekko architecture definitions (from datasheet).

#pragma once

// GC bus clock is running on 1/3 of CPU clock, and timer is
// running on 1/4 of bus clock (1/12 of CPU clock)
#define CPU_CORE_CLOCK  486000000u  // 486 mhz (its not 485, stop bugging me!)
#define CPU_BUS_CLOCK   (CPU_CORE_CLOCK / 3)
#define CPU_TIMER_CLOCK (CPU_BUS_CLOCK / 4)

#define BIT(n)              (1 << (31-n))

// Machine State Flags
#define MSR_RESERVED        0xFFFA0088
#define MSR_POW             (BIT(13))               // Power management enable
#define MSR_ILE             (BIT(15))               // Exception little-endian mode
#define MSR_EE              (BIT(16))               // External interrupt enable
#define MSR_PR              (BIT(17))               // User privilege level
#define MSR_FP              (BIT(18))               // Floating-point available
#define MSR_ME              (BIT(19))               // Machine check enable
#define MSR_FE0             (BIT(20))               // Floating-point exception mode 0
#define MSR_SE              (BIT(21))               // Single-step trace enable
#define MSR_BE              (BIT(22))               // Branch trace enable
#define MSR_FE1             (BIT(23))               // Floating-point exception mode 1
#define MSR_IP              (BIT(25))               // Exception prefix
#define MSR_IR              (BIT(26))               // Instruction address translation
#define MSR_DR              (BIT(27))               // Data address translation
#define MSR_PM              (BIT(29))               // Performance monitor mode
#define MSR_RI              (BIT(30))               // Recoverable exception
#define MSR_LE              (BIT(31))               // Little-endian mode enable

#define HID2_LSQE   0x80000000          // PS load/store quantization
#define HID2_WPE    0x40000000          // gathering enabled
#define HID2_PSE    0x20000000          // PS-mode
#define HID2_LCE    0x10000000          // cache is locked

#define WPAR_ADDR   0xffffffe0          // accumulation address
#define WPAR_BNE    0x1                 // buffer not empty

// Exception vectors (physical address)

#define CPU_EXCEPTION_RESET         0x0100
#define CPU_EXCEPTION_MACHINE       0x0200
#define CPU_EXCEPTION_DSI           0x0300
#define CPU_EXCEPTION_ISI           0x0400
#define CPU_EXCEPTION_INTERRUPT     0x0500
#define CPU_EXCEPTION_ALIGN         0x0600
#define CPU_EXCEPTION_PROGRAM       0x0700
#define CPU_EXCEPTION_FPUNAVAIL     0x0800
#define CPU_EXCEPTION_DECREMENTER   0x0900
#define CPU_EXCEPTION_SYSCALL       0x0C00
#define CPU_EXCEPTION_PERFMON       0x0D00
#define CPU_EXCEPTION_IABR          0x1300
#define CPU_EXCEPTION_THERMAL       0x1700
