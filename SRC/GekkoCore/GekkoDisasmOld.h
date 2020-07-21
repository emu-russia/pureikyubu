// See some documentation in CPP file.

#pragma once

// Instruction class
#define PPC_DISA_OTHER      0x0000  // No additional information
#define PPC_DISA_64         0x0001  // 64-bit architecture only
#define PPC_DISA_INTEGER    0x0002  // Integer-type instruction
#define PPC_DISA_BRANCH     0x0004  // Branch instruction
#define PPC_DISA_LDST       0x0008  // Load-store instruction
#define PPC_DISA_STRING     0x0010  // Load-store string/multiple
#define PPC_DISA_FPU        0x0020  // Floating-point instruction
#define PPC_DISA_OEA        0x0040  // Supervisor level
#define PPC_DISA_OPTIONAL   0x0200  // Optional
#define PPC_DISA_BRIDGE     0x0400  // Optional 64-bit bridge
#define PPC_DISA_SPECIFIC   0x0800  // Implementation-specific
#define PPC_DISA_ILLEGAL    0x1000  // Illegal
#define PPC_DISA_SIMPLIFIED 0x8000  // Simplified mnemonic is used

typedef struct PPCD_CB
{
    uint64_t   pc;                  // Program counter (input)
    uint32_t   instr;               // Instruction (input)
    char    mnemonic[16];           // Instruction mnemonic.
    char    operands[64];           // Instruction operands.
    uint32_t   immed;               // Immediate value (displacement for load/store, immediate operand for arithm./logic).
    int     r[4];                   // Index value for operand registers and immediates.
    uint64_t   target;              // Target address for branch instructions / Mask for RLWINM-like instructions
    int     iclass;                 // One or combination of PPC_DISA_* flags.
} PPCD_CB;

void    PPCDisasm(PPCD_CB *disa);
char*   PPCDisasmSimple(uint64_t pc, uint32_t instr);
