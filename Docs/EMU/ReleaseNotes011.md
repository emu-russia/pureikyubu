# Dolwin 0.11 Release Notes

This release is still in WIP status.

## What's new compared to 0.10

Fuh, it’s hard to tell in a nutshell, but I’ll try.

Firstly, the development has been restarted since 2004. Can you imagine what fossils you had to work with. Major changes:
- Multithreading: the emulator engine is prepared for multitask execution of all components. Now the interface, processor emulation, DSP and non-invasive debugger can simultaneously work on the background. All this can be stopped and run separately at any time.
- Basic emulation of GameCube DSP. It’s enough that the games do not hang at the very beginning. In future releases, support will be improved for sound output.
- The new JDI infrastructure has been added to the debugger (more details can be found here: https://github.com/ogamespec/dolwin/blob/master/Docs/EMU/JsonDebugInterface.md)
- The interface is translated to Unicode, the settings are now stored in Json. The settings themselves are greatly simplified.
- The source code is cleaned and divided into sub-projects

Plans for the next releases can be viewed on GitHub: https://github.com/ogamespec/dolwin/milestones

PS. This is not an April's joke, by the way Dolwin 0.10 was also released on April 1 :P

## Prerequisites

To run it requires DSP ROM dumps (known as dsp_irom.bin and dsp_coef.bin) and preferably GameCube IPL BIOS dumps (original 2 MByte encrypted dumps). Altered versions of IPL.bin are not supported.

Generally speaking, the launch of something is not guaranteed, as mentioned above, the emulator is still in the WIP stage :P

## Contacts

GitHub: https://github.com/ogamespec/dolwin
Discord: https://discord.gg/Ehz8PYA
Russian IRC channel: #emu-russia (NewNet)
