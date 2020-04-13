# Nintendo GameCube Emulator for Windows

![PNG](/Docs/ScreenShots/Bootrom_NTSC_Dolwin.png)

Dolwin is work-in-progress emulator of Nintendo GameCube console.

The goal of the emulator is to research the hardware features of GameCube and reverse engineer technologies used to develop games for this platform.
GameCube is the hardware masterpiece of Nintendo engineers and it's a pleasure to explore this device and discover something new for yourself.

This project is revived from 2004 in 2020.

## Building

Build using Visual Studio 2019. To build, open Dolwin.sln and click Build.

The executable will be at the root (Dolwin.exe).

## Solution structure

All solution projects are independent components (or try to be like that).

Currently, work is underway to encapsulate components in their namespaces, but the legacy code from version 0.10 does not make it so simple.

Currently, the following namespaces are quietly formed:

- Debug: for debugging functionality
- Gekko: for the core of the Gekko CPU
- Flipper: for various internal Flipper hardware modules (AI, VI, EXI, etc.)
- DSP: for GameCube DSP
- DVD: for a disk drive unit (DDU)
- GX: for the Flipper GPU

The user interface will most likely be rewritten as a Managed C# application. In the meantime, it's just like a piece of code from version 0.10.

## Third-party code

I try not to use third-party libraries without special need.

## Why "Dolwin"?

Dolwin was originally stands for "Nintendo *Dol*phin Emulator for *Win*dows" (Dolphin is GameCube codename).
Later "Dolphin" was changed to "GameCube", so that there is no confusion with another emulator - Dolphin-emu.

## Greets

We would like to say Thanks to people, who helped us to make Dolwin:
- Costis: gcdev.com and some valuable information
- Titanik: made GC development possible
- tmbinc: details of GC bootrom and first working GX demos
- DesktopMan: nice GC demos
- groepaz: YAGCD and many other
- FiRES and ector for Dolphin-emulator, nice chats and information
- Masken: some ideas from WhineCube
- monk: some ideas from gcube
- Alex Raider: basic Windows Console code
- segher: Bootrom descrambler
- Duddie: For DSP reversing and docs

And also to people, we have forgot or who wanted to stay anonymous :)

Many thanks to our Beta-testers, for bug and compatibility reports.
Dolwin Beta-team: Chrono, darkreign, Jeil, Knuckles, MasterPhW and Posty.

Thanks to Martin for web-hosting on Emulation64.com

## Contacts

- Official Discord channel: https://discord.gg/Ehz8PYA
- Skype: ogamespec
- Email: ogamespec@gmail.com
