# GCN Hardware

This component emulates everything inside the ASIC Flipper, *except* the graphics system (GX) and DSP.

![Flipper_Block_Diagram](https://github.com/ogamespec/dolwin-docs/blob/master/HW/Flipper_ASIC_Block_Diagram.png)

A short tour into the Flipper stuff, without shocking details:
- AI: Audio DMA
- AR: Aux. memory (ARAM) controller
- CP: Command Processor for GX FIFO
- EXI: SPI-like Macronix interface
- MI: Memory Interface
- PI: Processor Interface (interrupts, etc.)
- SI: Serial Interface (goes to GameCube controllers connectors)
- VI: Video output

Somehow also emulation of memory cards (Memcards) got here. For now, let it stay here, then we'll move it somewhere, if the emulation of more accessories and peripherals appears.

## Real revisions of Flipper

It is documented (in the Dolphin SDK) that there were at least 2 versions of Flipper hardware - `HW1` (an early debug version) and `HW2` (contained in the latest Devkits and Retail consoles).

## A small note on VI

GameCube uses a slightly alien image rendering engine:
- The scene is drawn by the GPU into the internal Flipper frame buffer (EFB)
- Then a special circuit (Copy Engine) copies this buffer to external memory on the fly converting RGB to YUV
- Buffer from external memory (XFB) is used by the video output circuit to directly send the picture to the TV

All this is wildly unoptimized in terms of emulation and is a lot of pain.

Therefore, emulator developers ignore the XFB output and display in the emulator what the GPU draws. With rare exceptions, this works in most games, but it does not work in Homebrew and the games of some perverse developers.

## GCNHardwareForPlayground

A special build version that defines Null backends for use in DolwinPlayground.
