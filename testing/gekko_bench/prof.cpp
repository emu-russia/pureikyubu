// Poor man's sampling profiler for the standalone Gekko benchmark.
// Records the interrupted instruction pointer (host) with SIGPROF; the addresses
// are resolved afterwards with addr2line.

#include <execinfo.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>
#include <cstdio>
#include <cstddef>

#define MAX_SAMPLES (4 * 1024 * 1024)

static void* g_samples[MAX_SAMPLES];
static volatile size_t g_sampleCount = 0;

static void ProfHandler(int sig, siginfo_t* info, void* ctx)
{
	ucontext_t* uc = (ucontext_t*)ctx;
	// The instruction pointer register of the interrupted host context. The harness
	// builds for both x86-64 and 32-bit x86 (see build.sh), and the two name it
	// differently in the Linux ucontext.
#if defined(__x86_64__)
	void* ip = (void*)uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__i386__)
	void* ip = (void*)uc->uc_mcontext.gregs[REG_EIP];
#else
	void* ip = nullptr;
#endif

	if (g_sampleCount < MAX_SAMPLES)
	{
		g_samples[g_sampleCount++] = ip;
	}
}

void ProfStart()
{
	struct sigaction sa{};
	sa.sa_sigaction = ProfHandler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction(SIGPROF, &sa, nullptr);

	struct itimerval tv{};
	tv.it_interval.tv_usec = 100;		// 10 kHz
	tv.it_value.tv_usec = 100;
	setitimer(ITIMER_PROF, &tv, nullptr);
}

void ProfStop(const char* outPath)
{
	struct itimerval tv{};
	setitimer(ITIMER_PROF, &tv, nullptr);

	FILE* f = fopen(outPath, "w");
	for (size_t i = 0; i < g_sampleCount; i++)
	{
		fprintf(f, "%p\n", g_samples[i]);
	}
	fclose(f);
	fprintf(stderr, "profiler: %zu samples -> %s\n", g_sampleCount, outPath);
}
