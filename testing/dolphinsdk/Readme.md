# DolphinSDK demo sweep (issue #385)

The Dolphin SDK ships ~150 demos (GX, G2D, OS, PAD, DVD, CARD, AX, ...). Most of them are small,
self-contained and make a very good regression suite for the emulator: each one exercises one
narrow hardware feature and its source says exactly what it is supposed to show.

This folder holds the methodology and the scripts that run the demos through the emulator's SDL2
port and collect enough evidence to judge the picture.

## 1. Build the SDL2 port

The sweep uses the windowed SDL2 front end (`uisdl.cpp` / `videosdl.cpp` / `gfx.cpp` with the
OpenGL backend), because the GX demos need a real GL context to draw and because the visual result
is the thing being checked.

```
MSBuild scripts\VS2026\pureikyubu.sln /p:Configuration="Release SDL" /p:Platform=x64
```

The output is `scripts\VS2026\x64\Release SDL\pureikyubu.exe`. Put it next to the runtime data,
together with `SDL2.dll` and `pong.dol`:

```
build\
    pureikyubu_sdl.exe     <- the build output
    SDL2.dll
    pong.dol
    Data\                  <- fonts, Bootrom ROMs, DSP ROMs, maps, settings
    autoexec.cmd
```

## 2. Mount the SDK folder as a virtual disc

`DVD::MountSdk` (`src/dvd.cpp`) builds a whole GameCube disc image in memory from a DolphinSDK
folder: the disk ID, the BI2 (`X86/bin/bi2.bin`), the apploader (`HW2/boot/apploader.img`),
`pong.dol` as the boot DOL, and an FST generated from the `dvddata` tree that is written out in
`MountDolphinSdk::dvdDataInfoText`. The demo files themselves are then read from the folder through
that FST, exactly as they would be read from a disc.

Mount it from the menu (**File -> Mount DolphinSDK as DVD...**) or, for an unattended run, from the
startup script the emulator executes on every load (`autoexec.cmd` in the working directory):

```
MountSDK C:\DolphinSDK
```

A successful mount is reported as:

```
[DVD] DolphinSDK mounted!
[DVD] Mounted SDK: C:\DolphinSDK
```

## 3. Make the demo's own report visible

The demos print their instructions and results with `OSReport`. The SDK's `OSReport` ends up in
`WriteUARTN` (`exi.a`), which only enables the serial port when `OSGetConsoleType()` says the
console is a devkit; on a retail board `InitializeUART` returns an error and the text is dropped.
The emulator's default console type is the retail one, so out of the box the text demos look like a
black screen.

Set the console type to the latest devkit in `Data\SettingsSdl.json` (the user settings file is read
from `Data\`, not from the working directory):

```json
{
    "hardware": { "CONSOLE": 268435462 }
}
```

`268435462` is `0x10000006`, "the latest Devkit HW" in the settings dialog. The report then reaches
the emulated UART and is written to the `EMU_LOG` on the `[Info]` channel:

```
[Info] Dolphin OS $Revision: 49 $.
[Info] Console Type : Development HW6
[Info] ************************************************
[Info] G2D-test: test game that uses 2D library
...
```

## 4. Run one demo

The SDK ships the demos only as ELF, so a demo is loaded as a file (`LoadELF`), not through the
disc's apploader:

```
pureikyubu_sdl.exe C:\DolphinSDK\HW2\bin\demos\gxdemo\smp-octa.elf
```

The emulator also accepts `--bench <file> <seconds>` for a bounded, unattended run that prints the
throughput report and exits.

## 5. Judge the result

For every demo:

1. **Read the source.** `DolphinSDK\build\demos\gxdemo\src\<category>\<demo>.c` says what the demo
   draws, which hardware feature it exercises and what the picture should look like.
2. **Take the picture.** Screenshot the emulator's `Video Output` window (`sweep.ps1` does it with
   `PrintWindow(PW_RENDERFULLCONTENT)`, so the shot is the window's own content and does not depend
   on the window being in front).
3. **Read the log.** `EMU_LOG=<file>` collects the hardware log; `pe finishes` counts the finished
   GX frames — a GX demo with `pe finishes: 0` never submitted a draw, and a picture that is one
   flat colour is a demo that drew nothing.
4. **Compare.** A demo is OK when the screenshot matches the source and the log shows the expected
   activity (frames, textures, DVD reads). Anything else is a finding, and the demo source plus the
   emulator source are then used to work out which side is wrong.

## 6. Scripts

| File | What it does |
|---|---|
| `sweep.ps1` | Runs a list of demos, captures the `Video Output` client area as a PNG, saves the `EMU_LOG` and the extracted `OSReport` text next to it, and writes a CSV summary |
| `summarize.py` | Turns a sweep folder into a table/JSON: frames per demo, unknown CP loads, exceptions, whether the shot is a single flat colour |

Example:

```
powershell -ExecutionPolicy Bypass -File testing\dolphinsdk\sweep.ps1 `
    -List demos_gx.txt -OutDir C:\Work\sweep_gx
python testing\dolphinsdk\summarize.py C:\Work\sweep_gx
```
