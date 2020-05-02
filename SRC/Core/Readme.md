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

## Exception Notes

We support: DSI(?), ISI(?), Interrupt, Alignment(?), Program, FP Unavail, Decrementer, Syscall.
(?) - only in advanced memory mode (MMU), or optionally.

### Common processing

- SRR0: PC (address where to resume execution, after 'rfi')
- SRR1: copied MSR bits.

### MSR processing

```
1. SRR[0, 5-9, 16-31] are copied from MSR.
2. disable translation - clear MSR[IR] & MSR[DR]
3. select exception base (not used in emu) :
    0x000nnnnn if MSR[IP] = 0
    0xFFFnnnnn if MSR[IP] = 1
4. MSR[RI] must be always 1 in emu!
5. on Interrupt and Decrementer MSR[EE] is cleared also.
```

## Gekko Wait Thread Queue

Wait Thread Queue is an optimization of threads that are tied to polling Gekko TBR value.

Without optimization, the threads in the loop check the TBR value until it reaches the required value, and then do their work and wait again.

With this design, the processor will spend resources on polling the TBR value.

The idea of optimization is put to sleep such threads until the TBR value is needed, then wake the thread up and it will immediately begin to do its job, without polling the TBR.

# Dolwin Recompiler Architecture

A recompiler or just-in-time compiler (JITC) is a widespread practice for optimizing emulators. Executable code of the emulated system (in this case, IBM Gekko) is translated into the executable code of the processor where the emulator is running (in this case, Intel X86/X64).

## Basics

The Dolwin recompiler translates code into sections called "segments". Each segment is a continuous section of Gekko code (that is, the segment ends on the first branch instruction, or any other instruction that non-linearly changes the Program Counter register).

All recompiled segments are stored in a cache arranged as `std::map`. The key is the starting address of the segment.

```c++
std::map<uint32_t, CodeSegment*> segments;
```

## Segment Translation

Segment translation is the actual process of recompiling a Gekko code segment into X86/X64 code.

Translation is carried out instruction by instruction, until the end of the segment (branch). The last branch is also translated.

Translation of individual instructions is carried out with the participation of the GekkoAnalyzer component. The instruction is analyzed, and then the structure with the analyzed information (AnalyzeInfo) is passed to the code generator of the corresponding instruction.

All instructions translators are located in the JitcX64 folder (for translating the X64 code) and JitcX86 (for translating the X86). In total, Gekko contains about 350 instructions, so there are many corresponding modules there :P

Code generation does not use any assemblers in order not to bloat source code. For these purposes, a small BitFactory class is used.

## Interpreter Fallback

At the initial stages of development (or for testing), it is possible to translate the code in such a way that the execution of the instruction is passed to the interpreter.

The code implementing this is in the Fallback.cpp module.

## Recompiler Invalidation

From time to time, an emulated program loads new software modules (overlays). In this case, the new code is loaded into the memory in place of the old code.

Gekko has a handy mechanism for tracking this. The `icbi` instruction is used to discard the old recompiled code.

The recompiler is also invalidated after setting or removing breakpoints.

## Running Recompiled Code

The Gekko run loop in recompilation mode looks something like this:

```c++

if (!SegmentCompiled(Gekko::PC))
{
	CompileSegment(Gekko::PC);
}

RunSegment(Gekko::PC);

// Check decrementer ...

// Check pending interrupt request ...

```

## Register Caching

There are advanced recompilation techniques where the register values of the previous segment instruction are used for the next. This technique is called register caching.

Dolwin does not support this mechanism, since modern X86/X64 processors are already advanced enough and contain various out-of-order optimizers and rename buffers. There is no need to do this work for them.
