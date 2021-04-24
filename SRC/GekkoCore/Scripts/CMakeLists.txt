add_library (GekkoCore SHARED 

	../Interpreter/NoAsm.cpp

	../Interpreter/Branch.cpp
	../Interpreter/Compare.cpp
	../Interpreter/Condition.cpp
	../Interpreter/FloatingPoint.cpp
	../Interpreter/FPLoadStore.cpp
	../Interpreter/Integer.cpp
	../Interpreter/LoadStore.cpp
	../Interpreter/Logical.cpp
	../Interpreter/PairedSingle.cpp
	../Interpreter/PSLoadStore.cpp
	../Interpreter/Rotate.cpp
	../Interpreter/Shift.cpp
	../Interpreter/System.cpp

	../JitcX64/Common.cpp
	../JitcX64/Fallback.cpp
	../JitcX64/JitcBranch.cpp
	../JitcX64/JitcInteger.cpp
	../JitcX64/JitcLoadStore.cpp
	../JitcX64/JitcPairedSingle.cpp
	../JitcX64/JitcPSLoadStore.cpp
	../JitcX64/JitcRotate.cpp

	../Breakpoints.cpp
	../Cache.cpp
	../GatherBuffer.cpp
	../Gekko.cpp
	../GekkoAnalyzer.cpp
	../GekkoCommands.cpp
	../GekkoDisasm.cpp
	../GekkoAssembler.cpp
	../Interpreter.cpp
	../Jitc.cpp
	../MemoryHub.cpp
	../Mmu.cpp
	../TLB.cpp
	../OpcodeStats.cpp

)
