add_library (DolwinEmu
	../Config.cpp
	../EmuCommands.cpp
	../Emulator.cpp
	../JdiServer.cpp
	../Loader.cpp
)

target_link_libraries (DolwinEmu LINK_PUBLIC Common)
