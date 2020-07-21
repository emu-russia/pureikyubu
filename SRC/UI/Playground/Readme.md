# Playground

This is the most portable version of the Dolwin build.

The emulation is loaded and started via the command line (`argv[1]` is path to the file that you want to run).

Debug messages (`DBReport`) are printed using `printf`.

The emulator core uses `Null` backends (AudioNull, VideoNull, GraphicsNull, PadNull).

You can use this code as a reference example to create your own Dolwin UI implementation.
