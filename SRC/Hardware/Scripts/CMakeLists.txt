add_library (Hardware SHARED 
	../AI.cpp
	../AR.cpp
	../CP_PE.cpp
	../DI.cpp
	../EXI.cpp
	../HW.cpp
	../HwCommands.cpp
	../IPLDescrambler.cpp
	../MC.cpp
	../MI.cpp
	../PI.cpp
	../SI.cpp
	../VI.cpp
)

target_link_libraries (Hardware LINK_PUBLIC VideoNull)
target_link_libraries (Hardware LINK_PUBLIC GraphicsNull)
target_link_libraries (Hardware LINK_PUBLIC AudioNull)
target_link_libraries (Hardware LINK_PUBLIC PadNull)
