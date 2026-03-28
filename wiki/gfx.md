# Flipper GFX

The entire emulation of the Flipper graphics Pipeline begins in the gfx.cpp module. It's not yet fully developed, but we plan to use two completely different approaches:
- Simulation of the entire GFX subsystem using vertex/fragment shaders
- Software emulation, with faithful copying of EFB -> XFB and other GFX blocks

The GFX subsystem is quite complex, even by 2026 standards, so it's not surprising that there's still a lot to be done.

## Emulation Approaches

![gfx_class](/wiki/imgstore/emu/gfx_class.png)

Currently, the GFX subsystem is heavily influenced by very old code. The first step was a refactoring to encapsulate the main GFX Pipeline modules as corresponding classes. We will then develop functionality within the class.
You can see that each class has a reference to the parent `gfx` instance. Overall, everything is currently mixed up and will be properly spaced.