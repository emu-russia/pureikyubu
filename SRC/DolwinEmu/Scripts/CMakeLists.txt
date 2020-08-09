add_library (DolwinEmu
	../Config.cpp
	../EmuCommands.cpp
	../Emulator.cpp
	../JdiServer.cpp
)

target_link_libraries (DolwinEmu LINK_PUBLIC Common)
