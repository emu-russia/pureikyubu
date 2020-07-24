# Backends

This section contains platform-specific code.

At the moment there are 4 types of backends:
- Audio mixer (e.g. AudioDirectSound)
- Emulation of the GameCube GPU pipeline (e.g. DolwinVideo)
- Output of raw video buffer (`XFB`) for Homebrew (e.g. VideoGdi). This is not a very necessary backend, as the Homebrew scene for the GameCube is not very active right now.
- Emulation of GameCube controllers (e.g. PadSimpleWin32)

## Null Backends

There are also dummy backends for maximum code portability.

They are used in DolwinPlayground so that developers can test memory leaks and bugs on key emulator components without being distracted by possible leaks and bugs in their UI / Backend implementations.

## Where's UI?

The user interface is placed above the emulator and its backends.

See the \\SRC\\UI folder for out-of-the-box UI implementation examples.
