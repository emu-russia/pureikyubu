# GCN Hardware

This component emulates everything inside the ASIC Flipper, *except* the graphics system (GX).

A short tour into the Flipper stuff, without shocking details:
- AI: Audio DMA
- AR: Aux. memory (ARAM) controller
- CP: Command Processor for GX FIFO
- EXI: SPI-like Macronix interface
- MI: Memory Interface
- PI: Processor Interface (interrupts, etc.)
- SI: Serial Interface (goes to GameCube controllers connectors)
- VI: Video output

There is also an emulation of memory cards (MC).

## A small note on VI

GameCube uses a slightly alien image rendering engine:
- The scene is drawn by the GPU into the internal Flipper frame buffer (EFB)
- Then a special circuit (Copy Engine) copies this buffer to external memory on the fly converting RGB to YUV
- Buffer from external memory (XFB) is used by the video output circuit to directly send the picture to the TV

All this is wildly unoptimized in terms of emulation and is a lot of pain.

Therefore, emulator developers ignore the XFB output and display in the emulator what the GPU draws. With rare exceptions, this works in most games, but it does not work in Homebrew and the games of some perverse developers.
