# プレイキューブ

<img src="wiki/imgstore/pureikyubu.png" alt="pureikyubu" width="620">

pureikyubu is a work-in-progress emulator of the Nintendo GameCube console.

The goal of the emulator is to research the hardware features of GameCube and reverse engineer the
technologies used to develop games for this platform. GameCube is the hardware masterpiece of
Nintendo/ArtX engineers, and it's a pleasure to explore this device and discover something new for
yourself.

## Progress

The emulator boots and runs more titles with every release — the per-title throughput of the current
release is listed in the [1.8 release notes](docs/RELEASE_NOTES_1.8.md). The screenshots below are
captures from earlier builds; the status bar shows the emulated MIPS, VI and PE rates. Some titles
still fail to render or hang — compatibility is a work in progress.

|![progress_bs2](wiki/imgstore/progress_bs2.png)|![progress_ikaruga](wiki/imgstore/progress_ikaruga.png)|![progress_luigi](wiki/imgstore/progress_luigi.png)|
|---|---|---|

## Build

### Windows

Build using Visual Studio 2026. Open `scripts/VS2026/pureikyubu.sln` and click Build. The solution
holds four projects: `pureikyubu` (the emulator), `SDL2`, `GBA` (the integrated Game Boy Advance and
Game Boy emulator, built as a library the emulator links) and `gba_bench` (the standalone harness of
the GBA core). Both the SDL and the Win32 front ends have Debug and Release configurations.

### Generic Linux (Ubuntu)

The Linux build does not yet have support for sound and input.

```
# Install required packages
sudo apt install libglew-dev
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