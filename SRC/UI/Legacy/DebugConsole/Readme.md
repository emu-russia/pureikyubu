# DebugConsole

The entire debug console UI has moved here.

This code is very oriented on the Win32 Console API, so it was natural to take it outside.

## Architectural features

Debug UI contains 2 instances:
- System-wide debugger focused on GekkoCore (code migrated from version 0.10)
- DSP debugger (the code is based on the more convenient Cui.cpp)

The Win32 Console API does not allow you to create more than one console per process, so you can only use one instance at a time.

Debug UIs can be opened via the Developement menu.
