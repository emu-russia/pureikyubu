# Flipper Processor Interface (PI)

This section describes the interface between the Gekko CPU and ASIC Flipper.

Information obtained from US Pat. No. 6,609,977 and Gekko User's Manual.

Gekko-Flipper interface is shown in the picture:

![Gekko-Flipper Interface](PI_001.png)

Signals description:

|Signal/group|Description|Direction|
|---|---|---|
|TRST|Must be also asserted in conjunction with HRESET|From Flipper|
|HRESET|Hardware Reset. Execution continues from 0xFFF0_0100 address|From Flipper|
|SYSCLK|486 Mhz Clock|To Gekko|
|INT|Interrupt request|From Flipper|
|TA|Transfer Acknowledge|From Flipper|
|AACK|Address Acknowledge (adress bus is latched by other side)|From Flipper|
|TSIZ0-2|Transfer Size (see below)|Set by Gekko|
|TBST|Transfer Burst. Additionaly used to send 32 Bytes|Set by Gekko|
|TT0-4|Transfer Type (see below)|Set by Gekko|
|TS|Transfer Start. Output: see TT0-4.|Set by Gekko|
|DL/DH0-31|64-bit Data Bus|Inout|
|A0-31|32-bit Address Bus|To Flipper|

### Transfer Size

The most important values:
- 2: Burst (32 Bytes)
- 0: 8 Bytes
- 1: 1 Byte
- 2: 2 Bytes
- 4: 4 Bytes

There is evidence and suspicion that for accessing the main memory, Flipper supports only 32-bit transactions and Burst (32 Bytes). Well, in fact, the remaining sizes are not so important for main memory, because interaction with the main memory occurs mainly through the cache (Burst operations).

### Transfer Type

Generally speaking, it’s not particularly interesting, since it is used for global synchronization between several cores and cache coherency. In fact, for the Gekko-Flipper bundle, only Single-beat read/write and Burst read/write are important.

## Conclusions

The operations performed on this interface are as follows:
- Memory Read / write (1, 2, 4, 8 and 32 bytes)
- Interrupt from Flipper (INT)
- Reset signal from Flipper

Burst transactions are used for the following purposes:
- Cache Fill
- Transfer to Flipper contents of Write-Gather Buffer

## Emulation Strategies

- Single beat read-write emulated by MIRead / MIWrite
- Burst Write-Gather Buffer transactions are transmitted in 32-byte packets for processing GX FIFO
- The cache is emulated simply: 2 copies of the main memory are created, one for Data Access and one for Instruction Access. The dcbst / dcbi / icbi instructions simply copy the contents from one buffer to another (from the cache to main memory and vice versa).
