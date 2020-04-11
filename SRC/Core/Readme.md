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
