# プレイキューブ

![pureikyubu](/imgstore/pureikyubu.png)

pureikyubu is work-in-progress emulator of Nintendo GameCube console.

The goal of the emulator is to research the hardware features of GameCube and reverse engineer technologies used to develop games for this platform.
GameCube is the hardware masterpiece of Nintendo/ArtX engineers and it's a pleasure to explore this device and discover something new for yourself.

## Build

### Windows version

Build using Visual Studio 2022. To build, open `scripts/pureikyubu.sln` and click Build.

### Generic Linux (Ubuntu) version

The Linux build does not yet have support for graphics, sound and a full Debug UI. All emulation output can be seen only through debug messages.

```
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

Requirements: CMake, pthread, OpenGL, imgui, SDL2.

## Progress

|![progress_bs2](/imgstore/progress_bs2.png)|![progress_ikaruga](/imgstore/progress_ikaruga.png)|![progress_luigi](/imgstore/progress_luigi.png)|
|---|---|---|

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

Official Discord channel: https://discord.gg/Ehz8PYA
