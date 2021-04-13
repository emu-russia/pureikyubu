# IntelCore

This component is a limited implementation of UVNA for Intel/AMD processors with x86/x64 architecture.
Support is implemented taking into account all the historical layers of this architecture so that the code can be reused in other projects. 

You can read about the UVNA concept here: https://github.com/ogamespec/dolwin-docs/blob/master/EMU/UVNA_en.md

Specifically, IntelCore only implements the `Assembler` for now, which is used to recompile the Gekko code.

Such a name was chosen with a reserve that the rest of UVNA will be implemented here (for example, `Disassembler`). 

## Technical features of x86/x64 instructions

Intel architecture actually hides three architectures (`modes`):
- DOS-era architecture (16-bit)   (And yes, we also support this mode, for possible reuse of the code in other projects)
- Protected mode architecture that was fully introduced in the 386 processor (32-bit)
- Long Mode architecture originally developed by AMD engineers but later adopted by Intel (64-bit)

All three architectures are the core on which the processor instructions are based, which are listed in the manual under the `General-Purpose Instructions` and `System Instructions` categories.

Accordingly, the first layer that must be implemented in `AnalyzeInfo` is the specified categories of instructions and the specifics of their format.

Then there are extensions, mainly related to speeding up the calculation of float/double/simd:
- x87 FPU Instructions
- MMX
- SSE
- AVX
- AVX-512

More extensions that have appeared relatively recently are BMI1 and BMI2.

These instruction format extensions can also be viewed as layers and implemented incrementally.

## General-Purpose and System Instructions

|Instruction format category |List of instructions |
|---|---|
|Instructions using ModRM (some instructions with an immediate operand also fall into this category)|ADC, ADD, AND, ARPL, BOUND, BSF, BSR, BT, BTC, BTR, BTS, CALL (in same segment), CALL (in other segment), CMP, CMPXCHG, DEC, DIV, IDIV, IMUL, INC, INVLPG, INVPCID, JMP (to same segment), JMP (to other segment), LAR, LDS, LEA, LES, LFS, LGDT, LGS, LIDT, LLDT, LDTR, LDTR, LMSW, LSL, LSS, LTR, MOV, MOVBE, MOVSX/MOVSXD, MOVZX, MUL, NOP (Multi-byte), NOT, OR, POP, PUSH, RCL, RCR, ROL, ROR, SAL, SAR, SBB, SETcc, SGDT, SHL, SHLD, SHR, SHRD, SIDT, SLDT, SMSW, STR, SUB, TEST, UD0, UD1, VERR, VERW, XADD, XCHG, XOR|
|Simple encoding instructions|BSWAP, IN, INT n, Jcc, JCXZ/JECXZ, LOOP, LOOPZ/LOOPE, LOOPNZ/LOOPNE, MOV (Control Registers), MOV (Debug Registers), OUT, POP (Segment Register), PUSH (Segment Register), RET (to same segment), RET (to other segment)|
|One or more byte instructions|AAA, AAD, AAM, AAS, CBW, CDQ, CDQE, CLC, CLD, CLI, CLTS, CMC, (REP/REPE/REPNE) CMPSB/CMPSW/CMPSD/CMPSQ, CPUID, CQO, CWD, CWDE, DAA, DAS, HLT, (REP/REPE/REPNE) INSB/INSW/INSD, INT 3, INTO, INT 1, INVD, IRET/IRETD/IRETQ, LAHF, LEAVE, (REP/REPE/REPNE) LODSB/LODSW/LODSD/LODSQ, (REP/REPE/REPNE) MOVSB/MOVSW/MOVSD/MOVSQ, NOP, (REP/REPE/REPNE) OUTSB/OUTSW/OUTSD, POPA/POPAD, POPF/POPFD/POPFQ, PUSHA/PUSHAD, PUSHF/PUSHFD/PUSHFQ, RDMSR, RDPMC, RDTSC, RDTSCP, RSM, SAHF, (REP/REPE/REPNE) SCASB/SCASW/SCASD/SCASQ, STC, STD, STI, (REP/REPE/REPNE) STOSB/STOSW/STOSD/STOSQ, SWAPGS, SYSCALL, SYSRET/SYSRETQ, UD2, WAIT, WBINVD, WRMSR, XLAT/XLATB|

## The situation with prefixes

The `prefixes` property in the `AnalyzeInfo` structure is used to set the prefixes.

## ModRM / SIB

Intel has historically used a very sophisticated addressing scheme, which is defined by a combination of ModRM and SIB byte fields (starting at 32-bit). Long Mode also adds the REX prefix, which further complicates the scheme.

This component abstracts whole kitchen by simply specifying the type of the parameter directly. For example the instruction:

```
adc [EAX * 2 + ECX + 0x11], EBX
```

is represented in the `AnalyzeInfo` structure with the following parameters:

- `Param::sib_eax_2_ecx_disp8`
- `Param::ebx`
- And additionally displacement is stored in the `Disp` property.

Thus, `Param` fully defines all possible combinations that can be specified using the ModRM/SIB/REX fields.

When compiling an instruction, the assembler takes into account the order of the parameters and compiles the corresponding opcodes and ModRM/SIB/REX bytes and the necessary prefixes (for example, if the specified instruction is compiled in 16-bit mode, the corresponding prefix will be added). 
