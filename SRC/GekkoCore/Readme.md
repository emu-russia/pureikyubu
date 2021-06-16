# GekkoCore

The main component that emulates the GameCube Gekko processor. GekkoCore components are shown in the diagram:

![GekkoCore](https://github.com/ogamespec/dolwin-docs/blob/master/EMU/GekkoCore.png?raw=true)

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

## MMU

GekkoCore supports MMU emulation, while it is still an experimental feature that requires debugging. Subsequently, as the MMU will work more or less correctly, 
the TLB functionality will gradually turn on.

TLB is the history of MMU translations, to speed up address translation. TLB is implemented as `std::unordered_map`.

## Cache Support

Supported data cache emulation and Locked L1 Data Cache.

Emulation of instruction cache and L2 Cache is not required. The instructions are always executed in one direction (Fetch), so there is no need to store them in the cache, 
you can immediately take them from memory. L2 Cache is also not required, since it works completely transparently for executable programs (there is no way to perform operations
with it using any instructions).

## Interpreter Architecture

The interpreter has been rewritten to use a generic decoder.

Each instruction handler already receives ready-made decoded information (`AnalyzeInfo`) and does not perform decoding. 

To speed up the operation of some instructions (Paired-Single Load Store and Rotate), pre-prepared tables are used.

## Recompiler Architecture

A recompiler or just-in-time compiler (JITC) is a widespread practice for optimizing emulators. Executable code of the emulated system (in this case, IBM Gekko) is translated into the executable code of the processor where the emulator is running (in this case, Intel X86/X64).

### Basics

The recompiler translates code into sections called "segments". Each segment is a continuous section of Gekko code (that is, the segment ends on the first branch instruction, or any other instruction that non-linearly changes the Program Counter register).

All recompiled segments are stored in a cache arranged as `std::unordered_map`. The key is the starting address >> 2 (all Gekko instructions are 4 byte aligned) of the segment.

```c++
std::unordered_map<uint32_t, CodeSegment*> segments;
```

### Segment Translation

Segment translation is the actual process of recompiling a Gekko code segment into X86/X64 code.

Translation is carried out instruction by instruction, until the end of the segment (branch). The last branch is also translated.

Translation of individual instructions is carried out with the participation of the GekkoAnalyzer component. The instruction is analyzed, and then the structure with the analyzed information (AnalyzeInfo) is passed to the code generator of the corresponding instruction.

All instructions translators are located in the JitcX64 folder (for translating the X64 code) and JitcX86 (for translating the X86). In total, Gekko contains about 350 instructions, so there are many corresponding modules there :P

The code is generated using the assembler of the IntelCore component.

### Interpreter Fallback

At the initial stages of development (or for testing), it is possible to translate the code in such a way that the execution of the instruction is passed to the interpreter.

The code implementing this is in the Fallback.cpp module.

### Recompiler Invalidation

From time to time, an emulated program loads new software modules (overlays). In this case, the new code is loaded into the memory in place of the old code.

Gekko has a handy mechanism for tracking this. The `icbi` instruction is used to discard the old recompiled code.

The recompiler is also invalidated after setting or removing breakpoints and several other cases.

The current executable segment is not invalidated, as this will lead to an exception.

### Running Recompiled Code

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

### Register Caching

There are advanced recompilation techniques where the register values of the previous segment instruction are used for the next. This technique is called register caching.

Dolwin does not support this mechanism, since modern X86/X64 processors are already advanced enough and contain various out-of-order optimizers and rename buffers. There is no need to do this work for them.

## Brief description of Gekko (PowerPC)

The GC CPU - Gekko is based on the PowerPC G3 (third generation) architecture, sources on the Internet point to the 750CXE model, but this is not particularly important, since Gekko is absolutely compatible with the 32-bit "Generic" PowerPC architecture. Generally speaking, in the PowerPC architecture, the processor model does not matter much, as it mainly affects performance. If a program is written with Generic PowerPC, it is guaranteed to run on all models. However, the models do differ from each other with additional "features" which can be used to improve the performance of programs.

Here is a list of what has been added "exclusively" to Gekko:
- Paired-Single instruction set;
- The cache can work in lock mode as a scratch-pad buffer. Swapping between the blocked-single cache can be done via DMA or direct write.
- Performance Counters. Counters for analyzing program performance. With their help the programmer can find out the number of executed instructions, hits/misses in cache and many other things;
- Level 2 cache (256 KB);
- Transition prediction block and transition memory table, for optimizing loops. (Those familiar with PowerPC assembler will find the "+" and "-" suffixes in the jump instructions useful);
- Write Gather Buffer. Quite often used for fast transfer of graphics data packets;
- Power Management. Three modes are available: DOZE, NAP and SLEEP. Although these modes are not used in games/programs and the Dolphin SDK;
- Debug interface for executing programs in steps and support for hardware breakpoints;

Now a brief description of what is available to the PowerPC programmer:
- The processor has 32 32-bit integer registers (denoted r0-r31) and 32 64-bit FPU registers (fr0-fr31). You can apply addition, subtraction, multiplication, division, arithmetic/logic shift operations and also logical operations (AND, OR, etc.) to integers. The same can be done with real numbers in IEEE-754 format of single or double precision (+, -, \*, /), in addition there is also the operation to calculate the square root and polynomial calculation A \* B + C. Special mention should be made of such a unique integer operation as `RLWINM` - which stands for "Rotate operand to the left by n bits, and then apply to it the AND mask", programmatically it can be written as `R = ROTL(A, n) & MASK`. This extremely powerful and versatile operation is widely used by the compiler to optimize C blocks, such as: `if((a >> 8) & 0xFF)` - this expression will be compiled as one(!) RLWINM instruction (including if check).
- There are only two modes of memory addressing: register + register and register + offset. Gentleman's kit.
- The processor operates in two modes: user and supervisor. The difference is that some purely system instructions and memory areas cannot be executed in user mode. In short, an analogy can be made here with the real and protected X86 mode.
- All system functions of the processor are accessed through Special-Purpose Registers.
- The CR (Condition Register) register is used to compare numbers. It contains 8 fields, each of which contains flags of comparison result. Immediately following the operation is a comparison instruction, which analyzes the state of the selected field register CR and make (or not make) transition. There is also such a convenient thing, how to compare the result of the current operation with zero. Assembler uses a dot suffix (".") to do this, e.g. execute an `add. r3, r4, r5` will go as follows: add r4 and r5, place the result in r3 and compare the result with zero. Place the result of the comparison in the `CR[0]` field.
- PowerPC has no "Jump" instructions. All jumps are done with "Branch" instructions which are numerous. Procedure calls are implemented via a special register - `LR` (Link Register). To call a procedure you need to execute the instruction "Branch And Link" which will save the return address in the `LR` register. The analog of "Return" is the instruction `blr` - "Branch to Link Register". Cycles like `FOR I=1 TO N` are implemented with the help of the `CTR` register (Counter). Special instruction of transition decreases `CTR` by 1 and makes transition according to the condition (`CTR` equals/not equals 0).

The instruction size is 32 bits. Disassembled PowerPC code looks like this:
```
8135D8A8  80A10008  lwz         r5, 8 (r1)
8135D8AC  8101000C  lwz         r8, 12 (r1)
8135D8B0  54A6007E  rlwinm      r6, r5, 0, 1, 31
8135D8B4  7C060000  cmpw        r6, r0
8135D8B8  90830000  stw         r4, 0 (r3)
8135D8BC  38E50000  addi        r7, r5, 0
8135D8C0  38860000  addi        r4, r6, 0
8135D8C4  4080000C  bge-        0x8135D8D0
8135D8C8  7C804379  or.         r0, r4, r8
8135D8CC  4082000C  bne-        0x8135D8D8
```
