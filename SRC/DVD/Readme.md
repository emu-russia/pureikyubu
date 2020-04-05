# DVD

This component implements everything you need for a healthy emulation of the GameCube disk drive unit (DDU).

There are currently two ways to read virtual DVDs:
- Read sectors of a mounted GC DVD image (GCM)
- Reading sectors of a virtual disk mounted as a DolphinSDK folder. Required for comfortable launch of DolphinSDK demos

## DDU Core

This component is designed to emulate the DDU controller. DduCore provides a host interface (Flipper) close to the actual DDU connector interface (P9).

That is, DduCore honestly tries to process transactions via DDU Bus, various control signals (BRK, RST), as well as emulation of the DVD Audio stream.

On the host side (/HW/DI.cpp), the DI part is implemented, which is responsible for transmitting a 12-byte DDU command and processing following transactions via the DDU Bus (in the Immediate buffer or Main Memory via DMA).

If you are going to understand DduCore, just imagine that the DduCore API is just signals from the DDU connector (P9).

The documentation for DiskInterface is located somewhere in Docs/HW.

## DDU Transaction Rate

In the official documentation there is a number of the minimum data transfer rate - 2 MByte/sec.

If the DDU does not manage to transmit the required amount of information and the Gekko Decrementer (OSAlarm) triggers before the DI TCINT interrupt, the game usually displays the message "Refer to the Instruction Booklet".
This means that the DDU does what is called "Babble."

Therefore, set the timings so that the transaction speed of DduCore fits into 2 MByte/sec in accordance with the Gekko TBR timer.

Since the processor and DDU threads work in parallel to avoid babbling when reading the image (fread), the ExecuteCommand method receives a hint from the command packet with the transaction size, so that it can pre-cache DVD data from the image.

## DVD Microcontroller(s)

It is not very clear yet how many controllers there are, how much RAM is inside the controller(s) and outside. Need to study the DDU motherboard.

But apparently the Firmware for this entire system is single - 128 KBytes. Dumps for several DDU models can be found on the internet.

- Matsushita MN103S13BGA Optical Disk Controller 
- Matsushita MN102H60GFA MicroComputer

MemoryMap:

|start|end|size|description|
|---|---|---|---|
|0x00008000| |4 Kbyte|internal (cpu) ram|
|0x00080000| |128 KByte|Firmware ROM. Reading the firmware at its reallocation is prevented by the debug commands (immbuffer will not be changed at all). however you can read its contents from the memory mirrors, ie 0x000a0000|
|0x00400000| | |internal (controller) ram|

(Source: YAGCD)

What is `cpu` and what is `controller` is not very clear. If there are two of them, then how does one get direct access to the memory of the other? And if the second one is also based on the processor core, then where is its firmware?

## DDU JDI

The debugging interface specification provided by this component can be found in Data\\DduJdi.json.
