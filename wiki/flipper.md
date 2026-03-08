# Flipper

![flipper_top_for_emu](/wiki/imgstore/emu/flipper_top_for_emu.png)

The emulator's modules are designed to closely match the GameCube architecture. Each Flipper HDL module is emulated by a corresponding .cpp module.

The image shows the interactions between Flipper's internal modules, as well as some external components (Gekko, audio, video, and the BootROM/RTC chip).

The black line represents the data bus between the Memory Interface and all other components. The blue lines indicate register operations between the PI and other components. The orange lines represent audio data coming from DVD Streaming (AIS) and samples from the DSP.
