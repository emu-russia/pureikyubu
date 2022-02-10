add_library (DSP SHARED 
	../DSP.cpp
	../DspAccel.cpp
	../DspAdpcm.cpp
	../DspDecoder.cpp
	../DspCommands.cpp
	../DspStack.cpp
	../DspAlu.cpp
	../DspCore.cpp
	../DspDisasm.cpp
	../DspDma.cpp
	../DspInterpreter.cpp
	../DspMailbox.cpp
	../DspMultiplyOps.cpp
	../DspParallelMemOps.cpp
	../DspParallelOps.cpp
	../DspRegularOps.cpp
)
