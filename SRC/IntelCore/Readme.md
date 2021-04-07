# IntelCore

This component is a limited implementation of UVNA for Intel/AMD processors with x86/x64 architecture.
Support is implemented taking into account all the historical layers of this architecture so that the code can be reused in other projects. 

You can read about the UVNA concept here: https://github.com/ogamespec/dolwin-docs/blob/master/EMU/UVNA_en.md

Specifically, IntelCore only implements the `Assembler` for now, which is used to recompile the Gekko code.

Such a name was chosen with a reserve that the rest of UVNA will be implemented here (for example, `Disassembler`). 

## Technical features of x86/x64 instructions

I will add a description of the format of the instructions here so that every time not climb into the 5000-page manual.

Intel architecture actually hides three architectures:
- DOS-era architecture (16-bit)
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

These instruction format extensions can also be viewed as layers and implemented incrementally.
