# Common

This section contains common API that have almost atomic significance for all projects.

- Json: Json serialization engine. Json is used to store emulator settings, as well as for the JDI system (Json Debug Interface).
- Spinlock: Mutually exclusive access synchronization.
- Thread: Portable threads.
- Jdi: Json Debug Interface. More information can be found in [JsonDebugInteface.md](https://github.com/ogamespec/dolwin-docs/blob/master/EMU/JsonDebugInterface.md)
- File: File utilities
- String: String utilities
- ByteSwap: Portable byte-swap API

# Note on Threads

Dolwin uses Suspend/Resume methods as control primitives.

The thread procedure is called `Worker`. Unlike conventional implementations, it does not contain an infinite loop, but simply makes one iteration of the thread.
The infinite loop is implemented above (in Common/Thread.cpp) to support the Suspend/Resume mechanism, where it is not supported by the native thread implementation (for example, in pthreads).
