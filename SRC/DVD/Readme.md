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

## DDU JDI

The debugging interface specification provided by this component can be found in Data\\DduJdi.json.
