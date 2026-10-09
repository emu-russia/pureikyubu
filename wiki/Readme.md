# プレイキューブ Wiki

This wiki contains information about the emulator for developers and anyone interested.

The wiki does not contain any detailed descriptions of the GameCube architecture, but is more focused on describing the emulator's internals.

* [Save states](savestate.md) — the whole machine in one file: the format and its sections, what a
  state carries and what it deliberately does not, the keys, the menu and the `savestate`/
  `loadstate`/`states` commands.

* [Idle wait skipping](idleskip.md) — what a title does at the end of a frame (`VIWaitForRetrace`
  and the yield loop behind it), how the recompiler recognises the wait, how much of a run it is,
  and what skipping it is worth on Metroid Prime.

* [GFX uniform cache](gfxuniformcache.md) — the shader pipeline uploads the whole register state of
  a draw to its two static programs, and `src/gfxuniformcache.cpp` drops the uploads whose value has
  not changed: what a cached value belongs to, when it is dropped, and what the tests pin down.

* [Guest frame profiler](guestprof.md) — the per-frame profiler: the units it charges
  host cycles to (every Gekko path, every Flipper block, the drive, the DSP, the interfaces and the
  host back ends), how the exclusive accounting works, and how a frame of an FMV is told apart from
  a frame the title drew from textures. See also [HW interface profiler](hwprof.md), whose channel
  table a frame record carries.

But here are some diagrams, just as a memo.

GameCube:

![gc_diagram](/wiki/imgstore/emu/gc_diagram.png)

Flipper:

![Flipper_ASIC_Block_Diagram](/wiki/imgstore/emu/Flipper_ASIC_Block_Diagram.png)

DSP:

* [DSP DROM](dsp_drom.md) — analysis of the 4 KiB DSP data ROM (`build/Data/dsp_drom.bin`):
  its palindromic quad tables, the interleaved lane structure and what the statistics say.
