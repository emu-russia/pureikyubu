# Flipper

![flipper_top_for_emu](/wiki/imgstore/emu/flipper_top_for_emu.png)

The emulator's modules are designed to closely match the GameCube architecture. Each Flipper HDL module is emulated by a corresponding .cpp module.

The image shows the interactions between Flipper's internal modules, as well as some external components (Gekko, audio, video, and the BootROM/RTC chip).

The black line represents the data bus between the Memory Interface and all other components. The blue lines indicate register operations between the PI and other components. The orange lines represent audio data coming from DVD Streaming (AIS) and samples from the DSP.

The Command Processor (cp) in the architecture is not directly part of the graphics pipeline and connects to gfx (shown in magenta) to transmit primitive drawing commands and load XF/SU/RAS/PE registers. In the emulator, cp is also served separately, working with pi to implement the alien GFX FIFO mechanism.