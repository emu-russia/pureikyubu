add_library (GekkoCore SHARED 

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

    ../Breakpoints.cpp
    ../Cache.cpp
    ../GatherBuffer.cpp
    ../GekkoCore.cpp
    ../GekkoDecoder.cpp
    ../GekkoDisasm.cpp
    ../Interpreter.cpp
    ../SixtyBus.cpp
    ../Mmu.cpp
    ../TLB.cpp
    ../OpcodeStats.cpp

)
