add_executable (DolwinPlayground 
	../DolwinPlayground.cpp 
	../DummyDebugger.cpp
	../UiCommands.cpp
	../UserJdiClient.cpp
)

target_link_libraries (DolwinPlayground LINK_PUBLIC Common)
target_link_libraries (DolwinPlayground LINK_PUBLIC VideoNull)
