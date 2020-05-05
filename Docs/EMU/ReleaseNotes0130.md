# Dolwin 0.130 Release Notes

The release with unlucky number 13 was split into two releases: 0.130 and 0.131. These superstitious programmers..

What's new:
- MMU support
- Support cache emulation
- Dynamic recompiler (JITC)
- Improved emulation of graphics FIFO
- Many other minor improvements

All these things were added experimentally and at the moment the cache and recompiler are temporarily disabled. If you are a developer, you can rebuild Dolwin with the cache and recompiler turned on.

The cache is enabled with the command `CacheDebugDisable 0`.

The recompiler is turned on in SRC\\Core\\Gekko.cpp, line 20 (but the interpreter must be disabled).

Between 0.130 and 0.131, I will try to fix all incomprehensible bugs with cache and recompiler so that they are included in the next release.

## Requirements

- Dolwin makes heavy use of multicore multithreading. Therefore, it is desirable that your processor contains 4 or more cores.
- The memory requirements are not so strict, a few gigabytes should be enough
- Emulation requires DSP IROM / DROM dumps
- A BIOS image dump is not required, but if you want to experiment with it, you can also add it in the settings. The BIOS is launched through the menu File -> Run Bootrom. Then you need to wait a bit and open the drive cover (File -> Swap Disk -> Open Cover). After that, IPL Menu will start :p

## What happens

Overall, the GameCube emulation has made significant progress. Games such as Ikaruga, 18 Wheeler, Super Monkey Ball, and for example Ed, Edd and Eddy are launched.

However, not all of them reach Ingame, contain graphic bugs and suffer from lags.

Most games still do not start due to insufficiently accurate emulation of the DSP or GPU.

The next release (0.131) is aimed precisely at eliminating all the shortcomings of DSP emulation, so that at last you can launch such top games as Legend of Zelda.
