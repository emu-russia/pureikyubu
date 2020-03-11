# Nintendo GameCube Emulator for Windows

![PNG](/whc4e0b53d99e258.png)

Dolwin is an emulator of Nintendo Gamecube console.
This platform is based on PowerPC-derivative processor, produced by IBM Corp. and codenamed Gekko.
Dolwin mainly purposed to emulate homebrewn applications.

Emulator is using such techinques as interpreter and just-in-time compiler.
It has debugger interface to trace emulated application. Dolwin has friendly user interface and file list selector.

Hardware emulation is based on plugin system. There are graphics, sound, audio and DVD plugins. All plugins are also included in project.

Dolwin supports high-level emulation (known as HLE) of Gamecube operating system and some additional system calls.

Dolwin is composed as project for Microsoft Visual Studio. Main language is plain C, with some x86 assembly optimisations.

<h2>Why "Dolwin" ?</h2>

Dolwin originally stands for "Nintendo <code>*Dol*</code>phin Emulator for <code>*Win*</code>dows" (Dolphin is Gamecube codename).
Later "Dolphin" was changed to "Gamecube", чтобы не было путаницы с другим эмулятором - Dolphin-emu.

This project is revived from 2004 in 2020.
