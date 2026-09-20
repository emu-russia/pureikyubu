# プレイキューブ

<img src="wiki/imgstore/pureikyubu.png" alt="pureikyubu" width="620">

pureikyubu is a work-in-progress emulator of the Nintendo GameCube console.

The goal of the emulator is to research the hardware features of GameCube and reverse engineer the
technologies used to develop games for this platform. GameCube is the hardware masterpiece of
Nintendo/ArtX engineers, and it's a pleasure to explore this device and discover something new for
yourself.

## Progress

The emulator boots and runs more titles with every release — the per-title throughput of the current
release is listed in the [1.9.1 release notes](docs/RELEASE_NOTES_1.9.1.md). Release 1.9 added a
second machine (an integrated Game Boy Advance and Game Boy, in `src/gba`), a software (CPU)
rendering path next to the OpenGL one, a new debugger and a local MCP server; 1.9.1 makes the
portable machines sound and time the way the hardware does, fixes the host-side BIOS, and leaves the
SDL2 front end as the only one. The screenshots below are captures from earlier builds; the status
bar shows the emulated MIPS, VI and PE rates. Some
titles still fail to render or hang — compatibility is a work in progress.

|![progress_bs2](wiki/imgstore/progress_bs2.png)|![progress_ikaruga](wiki/imgstore/progress_ikaruga.png)|![progress_luigi](wiki/imgstore/progress_luigi.png)|
|---|---|---|

## Build

### Windows

Build using Visual Studio 2026. Open `scripts/VS2026/pureikyubu.sln` and click Build. The solution
holds four projects: `pureikyubu` (the emulator), `SDL2`, `GBA` (the integrated Game Boy Advance and
Game Boy emulator, built as a library the emulator links) and `gba_bench` (the standalone harness of
the GBA core). The emulator has Debug and Release configurations for both platforms and is the SDL2
front end on both of them (issue #421 removed the Win32 one, so there is no second port with a
configuration of its own any more).

"Both platforms" means x64 and x86 (the IDE calls the latter Win32): the same sources build a 32-bit
emulator, and the recompilers follow the host - `gekkojit_x64.cpp` + `gekkojit_ps_x64.cpp` and
`dspjit_x64.cpp` on x64, `gekkojit_x86.cpp` + `gekkojit_ps_x86.cpp` and `dspjit_x86.cpp` on x86 (see
`src/gekkojit.h` and `src/dspjit.h`). The projects target Windows 7 (`_WIN32_WINNT=0x0601`), which
is the oldest Windows SDL2 2.28 and the emulator itself run on - no API newer than Windows 7 is
used anywhere in the emulator sources.

### Headless

`scripts/VS2026/pureikyubu_headless.vcxproj` is the emulator without a window: no SDL/ImGui front
end, no OpenGL context, no controller, audio or video output. It is a console application meant for
unattended runs — a build server, a scripted test, a DolphinSDK demo sweep, a benchmark. It is part
of the solution (so it is easy to find in the IDE) but is not built by "Build Solution"; build it
directly:

```
MSBuild scripts/VS2026/pureikyubu_headless.vcxproj -p:Configuration=Release -p:Platform=x64
```

On Linux the same target is built with `cmake -DHEADLESS=ON ..`.

The command line options are the usual ones; the difference is that this build has no game
selector, so an image, the Bootrom or a benchmark has to be requested:

```
pureikyubu_headless "D:\Isos\game.gcm"      # run it until Ctrl+C
pureikyubu_headless --ipl                   # run the Bootrom
pureikyubu_headless --bench game.gcm 30     # measure it and print the report
pureikyubu_headless --mcp                   # serve the debug interface to an MCP client
```

The reports are also written to the console and to the `EMU_LOG` file, and `--help` lists the
accepted options. `--mcp` runs the emulator as a local MCP server (an LLM agent starts it and
drives its whole debug interface, see [wiki/mcp.md](wiki/mcp.md)).

### Generic Linux (Ubuntu)

The Linux build is the same SDL2 front end as the Windows one: the window, the game selector, the
settings and controller dialogs, the sound and the input all come from SDL2 and ImGui.

```
# Install required packages
sudo apt install libglew-dev libsdl2-dev
# Choose a suitable folder to store a clone of the repository, cd there and then
git clone https://github.com/emu-russia/pureikyubu.git
cd pureikyubu
git submodule init
git submodule update
cd build
cmake ..
make
./pureikyubu pong.dol
```

Requirements: CMake, pthread, OpenGL, GLEW, imgui, SDL2. If cmake says that some components cannot
be built, you should look for solutions on the Internet (`apt get install xxx`) as usual.

You can test your Linux build on Windows using WSL2. Recently, you can also run graphical
applications (SDL2) there.

## AI usage policy

<img src="wiki/imgstore/ai_policy_invader.png" align="right" width="168" alt="Space Invader">

Because of the categorical reception AI gets in some communities, I decided to write a few words
about this project's policy on the use of AI (agentic coding).

The industry has already developed a mature attitude towards AI: it is a tool, and using it well is
a skill of its own.

On this project, agentic programming is applied both to improve functionality (new features) and to
find and fix bugs. A few points I would like to make:

- **Modern agentic systems are far from the neural-slop generators they are made out to be.** They
  are a solid senior-level partner you share an understanding, a code style and a pattern-mind with.
  The Luddites who brand everything as slop will simply be filtered out over time.
- **Is the emulator's source code unique?** Yes, it is. In the technical specifications I
  deliberately write prompts so that the agent does not peek at other emulators. This is a hard
  rule. Besides, those who have been with us for a while know that the emulator has a large code
  base that is self-sufficient for holding context (developed debugging facilities, an auxiliary
  GameCube specification).
- **The agent does not work autonomously.** Development is not done in a loop at the moment. The
  meatbag assists the agent at every stage (the share of rejected incorrect agent trajectories is
  currently around 30%). The results are reviewed, and in places finished by hand.

<br clear="right">

## Credits

We would like to say Thanks to people, who helped us to make Dolwin/pureikyubu:

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

Dolwin 0.10 Team:

- hotquik (http://www.hotsoft.com.ve/about/), responsible for memory cards emulation, Bootrom fonts and UI.
- org (ogamespec), responsible for the rest

## Contacts

- Official Discord channel: https://discord.gg/Ehz8PYA
- Source code: https://github.com/emu-russia/pureikyubu
- Issues and compatibility reports: https://github.com/emu-russia/pureikyubu/issues