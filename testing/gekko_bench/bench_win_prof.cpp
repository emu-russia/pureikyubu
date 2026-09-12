// The POSIX sampling profiler is not available on Windows; the harness only calls
// ProfStart/ProfStop from the BENCH_PROF path, so empty stubs keep it portable.
void ProfStart() {}
void ProfStop(const char* outPath) { (void)outPath; }
