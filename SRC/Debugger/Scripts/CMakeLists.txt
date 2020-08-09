add_library (Debugger
	../DebugCommands.cpp
	../EventLog.cpp
	../Report.cpp
	../SamplingProfiler.cpp
)

target_link_libraries (Debugger LINK_PUBLIC GekkoCore)
