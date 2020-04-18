# SamplingProfiler

Utility application for analyzing data collected using the Sampling Profiler (`StartProfiler` and `StopProfiler` commands).

## Controls

Input data:
- Json with collected samples (sampleData)
- GameCube main memory dump (can be obtained using the `ramsave` command). It should be kept in mind that if the program loaded overlay during the collection of samples, then the code that was sampled before may differs to the sampled addresses. I have not yet figured out how it is more convenient to make support for overlays (DolphinSDK REL files)
- Symbolic information (Map). Supports CodeWarrior and Dolwin RAW map formats.

More information can be found in Docs\\EMU folder.
