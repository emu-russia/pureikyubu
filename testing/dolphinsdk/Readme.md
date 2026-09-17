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

### 5.1. Compare with the source and the specifications, never with the emulator

Many demos draw their own state - a mode, a parameter, a colour - and therefore carry the expected
values in the picture: a TEV test, for instance, prints the operand each of its arguments selects
and the operation, bias and scale it applies, so a shot says what the stage should have computed.
The comparison that finds bugs is the one against a model built from the demo source and the
specifications; a model taken from the emulator's own behaviour can only agree with it.

The recipe:

* transcribe what the demo does with its state - a short script that replays the button handling of
  the demo's `AnimTick` is enough, and then the state behind every shot is known from the C file
  instead of being guessed from the picture;
* derive the expected value from the documentation, and where the documentation is thin, from the
  design description the documentation was written from;
* sample the demo's own readout out of the screenshot (the panel geometry is fixed in the source)
  and compare panel by panel, which also catches an operand the demo selects but the emulator maps
  elsewhere;
* let the same model generate the *steps* to press, so the sweep covers the whole reachable state
  space rather than one hand-picked picture.

The rule applies to the unit tests as well: a reference model such as the `TevRef` of
`testing/gfx_tev_test.cpp` has to be written from the documentation, because a reference that
mirrors the emulator keeps passing however wrong the emulator is. That is how the two arithmetic
details of the TEV combine were found - the tests had been written from the emulator's own formula,
so they could not disagree with it.

### 5.2. The register-level ground truth

A picture is the result of the whole pipeline, so when it disagrees with the model the next question
is what the GX library actually programmed. The emulator answers that itself: `gxregs` dumps the
state of every block as Json, and `--mcp` puts the whole debug interface behind a script
(`mcp_keys.ps1` starts a demo with the server, presses pad keys and prints the command's answer).
That is how the TEV swap tables and the K-constant selects of `TEV_KSEL` were read out, and it is
the only way to tell a register a *title* programmed from a register the *emulator* invented.

## 6. Driving the pad (interactive demos)

Most demos are not one picture: the pad picks the mode, the pattern or the parameters, and a sweep
of their idle state sees only the first of them. `pad_harness.ps1` runs one demo, presses a
scripted key sequence on the emulated controller and shoots the window after every step, so each
mode can be captured and checked against the source.

The keys are injected with `SendInput` into the emulator's own SDL keyboard bindings (`SettingsSdl.json`,
the `controllers` section) - the same path a human uses:

| pad | key | binding |
|---|---|---|
| X / Y | `S` / `A` | `VKEY_FOR_X` / `VKEY_FOR_Y` |
| A / B | `X` / `Z` | `VKEY_FOR_A` / `VKEY_FOR_B` |
| R | `W` | `VKEY_FOR_TRIGGERR` |
| START | Enter | `VKEY_FOR_START` |
| D-PAD | Delete / PageDown / Home / End | `VKEY_FOR_LEFT` / `RIGHT` / `UP` / `DOWN` |

Two traps, both found the hard way:

* a posted `WM_KEYDOWN` does not reach the emulated pad at all, `SendInput` does - and it needs the
  `Video Output` window in the foreground, which the harness arranges before every press;
* an injected *stick* key (the arrow keys) does not arrive either, while the D-PAD keys do. A demo
  whose cursor reads only the stick (`DEMOPadGetDirsNew`, as in `tev-swap`) can therefore be driven
  through its buttons only - which is enough for its swap-table menu, because the cursor starts on
  the entry that cycles the parameter.
* a pad sweep has to be the only thing using the machine. The window must stay in the foreground
  (anything that opens another window - a concurrent test run, for instance - takes the keys away),
  and a real controller plugged into the machine drives the emulated pad too (`padsdl.cpp`), so a
  hand on the pad changes the state the shot is supposed to show. Two runs were lost that way and
  had to be thrown away.

```
powershell -ExecutionPolicy Bypass -File testing\dolphinsdk\pad_harness.ps1 `
    -Demo C:\DolphinSDK\HW2\bin\demos\gxdemo\tev-one-op.elf -OutDir C:\Work\tev `
    -Steps C:\Work\tev\steps.json
```

`-Steps` is a JSON array of `{name, keys, hold, wait}`: the harness holds each key, waits for the
frame to catch up and captures `NNN_<name>.png`. Write the script from the demo's source: the
buttons it reads (`DEMOPadGetButtonDown`, `DEMOPadGetDirsNew`) and what it does with them are the
whole specification of what can be swept.

## 7. Scripts

| File | What it does |
|---|---|
| `sweep.ps1` | Runs a list of demos, captures the `Video Output` client area as a PNG, saves the `EMU_LOG` and the extracted `OSReport` text next to it, and writes a CSV summary |
| `pad_harness.ps1` | Runs one demo, drives the pad with a scripted key sequence (`-Steps`) and captures a shot per step |
| `mcp_keys.ps1` | Starts a demo with the MCP server, presses pad keys and runs one debug command (a `gxregs` register dump, for instance) |
| `summarize.py` | Turns a sweep folder into a table/JSON: frames per demo, unknown CP loads, exceptions, whether the shot is a single flat colour |
| `report.py` | Builds `report.html` from a sweep folder, `notes.json` and an output directory; with a fourth argument (a second sweep folder) it adds the **same demos rendered by the software GFX pipeline** side by side and marks every picture that differs from the shader backend's |
| `notes.json` | The per-demo analysis of the interesting cases |
| `shots/` | The screenshots `report.html` embeds (the shader backend) |
| `shots_soft/` | The same demos through the software pipeline of issue #384 (`GFX_PIPELINE = 1`) |

Example:

```
powershell -ExecutionPolicy Bypass -File testing\dolphinsdk\sweep.ps1 `
    -List demos_gx.txt -OutDir C:\Work\sweep_gx
python testing\dolphinsdk\summarize.py C:\Work\sweep_gx

rem ... and the same demos through the software GFX pipeline, then the two-panel report
powershell -ExecutionPolicy Bypass -File testing\dolphinsdk\sweep.ps1 `
    -List demos_gx.txt -OutDir C:\Work\sweep_gx_soft `
    -Exe C:\Work\pureikyubu\scripts\VS2026\x64\Release SDL\pureikyubu.exe
python testing\dolphinsdk\report.py C:\Work\sweep_gx testing\dolphinsdk\notes.json `
    testing\dolphinsdk C:\Work\sweep_gx_soft
```

The software-pipeline sweep needs `"GFX_PIPELINE": 1` in the `hardware` section of
`Data\SettingsSdl.json` (or `gxpipeline soft` in the debugger); the shader sweep is the default.
Both runs use the same emulator build, the same settle time and the same demo list, so a difference
between the two columns is a difference between the two rendering paths.
