# GekkoCore

This core is in transition state from the old 0.10 sources to a more advanced multi-threaded approach.

## A few key features

The Gekko core in the emulator is the ringleader for all other temporary processes.
The remaining components are also multi-threaded, but do not live on their own. They all dance from the Gekko internal timer - Time Base Register (TBR).

The emulated core conditionally executes 1 instruction per 1 TBR tick. Thus, an ideal core executes 486,000,000 instructions / ticks per second. 
In reality, this value may be less, maybe more, but all virtual time is counted from the TBR base anyway.

The DSP core waits for a certain number of ticks to do its job. The Flipper VI emulation module waits for a certain number of TBR ticks to generate a VBlank interrupt. Etc.

If with such a scheme of work the system will produce more frames than necessary, we will artificially slow it down (by delays).
But while the core is based on an interpreter - the speed is about 10-20 FPS :P

## Why assembler modules are needed

Used in the interpreter to easily get the Overflow and Carry flags. They are obtained directly from your X86 / X64 processor.
