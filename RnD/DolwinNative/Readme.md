# DolwinNative

Native part of Dolwin to run in managed environment.

When Dolwin starts in a managed environment, the interop library calls EMUCtor at boot and EMUDtor at exit.

All other interactions are made through JDI calls (CallJdi).
