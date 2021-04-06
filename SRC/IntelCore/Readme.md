# IntelCore

This component is a limited implementation of UVNA for Intel/AMD processors with x86/x64 architecture.
Support is implemented taking into account all the historical layers of this architecture so that the code can be reused in other projects. 

You can read about the UVNA concept here: https://github.com/ogamespec/dolwin-docs/blob/master/EMU/UVNA_en.md

Specifically, IntelCore only implements the `Assembler` for now, which is used to recompile the Gekko code.

Such a name was chosen with a reserve that the rest of UVNA will be implemented here (for example, `Disassembler`). 
