# Overview

After restarting the development, the architecture of the emulator has changed a bit.

Now all the main components (like processor, debugger, etc.) use multithreading.

Also, all components are divided into Visual Studio projects, which are a dependency for the main Dolwin.exe project.

Visual Studio solution upgraded to VS2019. There should not be a problem with the build.

Both X86 and X64 targets are supported.

## Trends

- Emulator development has become more Agile. Basic milestones can be viewed on Github (https://github.com/ogamespec/dolwin/milestones)
- The source code began to contain more c++, but without fanaticism (limited to namespaces and simple container classes)

## Solution structure

All solution projects are independent components (or try to be like that).

Currently, work is underway to encapsulate components in their namespaces, but the legacy code from version 0.10 does not make it so simple.

Currently, the following namespaces are quietly formed:

- Debug: for debugging functionality
- Gekko: for the core of the Gekko CPU
- Flipper: for various internal Flipper hardware modules (AI, VI, EXI, etc.)
- DSP: for GameCube DSP
- DVD: for a DVD unit (now the functionality is limited to reading images of ISO discs)
- GX: for the Flipper GPU

The user interface will most likely be rewritten as a Managed C# application. In the meantime, it's just like a piece of code from version 0.10.

## What's in the Docs folder

There are accompanying materials that were used in the development process. The HW folder contains more or less coherent GAMECUBE documentation. In the RE folder is located Reverse Engineering of various software for GAMECUBE, as well as DSP UCodes and IPL BIOS reversing.

Old descriptions contain a lot of "Runglish", newer ones I compose using machine translation (Google Translate), so reading them is not so painful.

All documents are quietly redone as Markdown.

I also try to remove the silly style of presentation everywhere. We got older :p

Graphic schemes are created in the yEd (https://www.yworks.com/products/yed).

## RnD

For experimental and outdated code, an RnD folder was created.
