add_executable (DolwinPlayground 
	../DolwinPlayground.cpp 
	../DummyDebugger.cpp
	../UiCommands.cpp
	../UserJdiClient.cpp
)

target_link_libraries (DolwinPlayground LINK_PUBLIC VideoNull)
target_link_libraries (DolwinPlayground LINK_PUBLIC GraphicsNull)
target_link_libraries (DolwinPlayground LINK_PUBLIC AudioNull)
target_link_libraries (DolwinPlayground LINK_PUBLIC PadNull)

target_link_libraries (DolwinPlayground LINK_PUBLIC Common)
target_link_libraries (DolwinPlayground LINK_PUBLIC Debugger)
target_link_libraries (DolwinPlayground LINK_PUBLIC DolwinEmu)
target_link_libraries (DolwinPlayground LINK_PUBLIC DSP)
target_link_libraries (DolwinPlayground LINK_PUBLIC DVD)
target_link_libraries (DolwinPlayground LINK_PUBLIC GekkoCore)
target_link_libraries (DolwinPlayground LINK_PUBLIC Hardware)
target_link_libraries (DolwinPlayground LINK_PUBLIC HighLevel)
