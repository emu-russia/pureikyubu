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

if (X86)
    ../JitcX86/Common.cpp
    ../JitcX86/Fallback.cpp
    ../JitcX86/JitcBranch.cpp
    ../JitcX86/JitcCompare.cpp
    ../JitcX86/JitcCondition.cpp
    ../JitcX86/JitcFloatingPoint.cpp
    ../JitcX86/JitcFPLoadStore.cpp
    ../JitcX86/JitcInteger.cpp
    ../JitcX86/JitcLoadStore.cpp
    ../JitcX86/JitcLogical.cpp
    ../JitcX86/JitcPairedSingle.cpp
    ../JitcX86/JitcPSLoadStore.cpp
    ../JitcX86/JitcRotate.cpp
    ../JitcX86/JitcShift.cpp
    ../JitcX86/JitcSystem.cpp
else ()
    ../JitcX64/Common.cpp
    ../JitcX64/Fallback.cpp
    ../JitcX64/JitcBranch.cpp
    ../JitcX64/JitcCompare.cpp
    ../JitcX64/JitcCondition.cpp
    ../JitcX64/JitcFloatingPoint.cpp
    ../JitcX64/JitcFPLoadStore.cpp
    ../JitcX64/JitcInteger.cpp
    ../JitcX64/JitcLoadStore.cpp
    ../JitcX64/JitcLogical.cpp
    ../JitcX64/JitcPairedSingle.cpp
    ../JitcX64/JitcPSLoadStore.cpp
    ../JitcX64/JitcRotate.cpp
    ../JitcX64/JitcShift.cpp
    ../JitcX64/JitcSystem.cpp
endif ()

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
