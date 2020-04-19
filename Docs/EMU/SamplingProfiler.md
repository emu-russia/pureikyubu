# Sampling Profiler

This profiler is used to search for bottlenecks in emulated programs (games).

If you are already familiar with the Visual Studio profiler, then understanding this profiler implementation will be easier.

## Profiling method

The profiler runs on its own thread.

After starting the profiler, it starts at a certain interval of the Gekko program timer (TBR) to poll the value of the Program Counter register.

The polling frequency is set by the profiler parameters, usually a frequency of 1 msec is enough.

A pair of sampled TBR + PC values are stored in Json.

After the profiler finishes, the accumulated data is serialized to a Json file.

## Data analysis

The resulting data is loaded into the RnD Profiler application.

The application splits the address space into segments of 32 bytes in size and generates a list of the frequency and duration of the processor in this segment.

The frequency is obtained by the value of Program Counter, and the duration by the value of TBR.

Thus, it is possible to understand where the emulated program was executed longer / most often.

In addition, you can load a file with symbolic information (Map) to see function names instead of addresses.
