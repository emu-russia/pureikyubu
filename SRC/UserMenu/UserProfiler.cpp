// emulator is profiled every real-time second.
// status bar should present and contain at least two sections :
// first (largest) is used for frame timing percentage
// second (small) is used for FPS
#include "dolphin.h"

//
// local data
//

static  BOOL    Profiler;   // uservar

static  s64     startTime, stopTime;
static  s64     gfxStartTime, gfxStopTime;
static  s64     sfxStartTime, sfxStopTime;
static  s64     padStartTime, padStopTime;
static  s64     dvdStartTime, dvdStopTime;
static  s64     cpuTime, gfxTime, sfxTime, padTime, dvdTime, idleTime;
static  s64     ONE_SECOND;
static  s64     fpsTime, mipsTime;
static  DWORD   checkTime;

// ---------------------------------------------------------------------------

// precede timer utility
// stupid msdev hungs on asm { ... } blocks
// so use single-line __asm expressions

static __declspec(naked) void __fastcall MyReadTimeStampCounter(s64 *ptr)
{
    // rdtsc
    __asm  _emit    0x0f
    __asm  _emit    0x31
    __asm   mov     [ecx], eax
    __asm   mov     [ecx + 4], edx
    __asm   ret
}

// get average CPU speed (in MHz)
static float GetClockSpeed()
{
    int i = 0;
    LARGE_INTEGER   t0, t1, perfFreq;
    s64 stamp0 = 0, stamp1 = 0;
    s64 diffStamp, diffTicks;

    if(QueryPerformanceFrequency(&perfFreq))
    {
        while(i < 3)
        {
            i++;

            QueryPerformanceCounter(&t0);
            t1.LowPart = t0.LowPart;
            t1.HighPart = t0.HighPart;

            while((t1.LowPart - t0.LowPart) < 50)
            {
                QueryPerformanceCounter(&t1);
                MyReadTimeStampCounter(&stamp0);
            }

            t0.LowPart = t1.LowPart;
            t0.HighPart = t1.HighPart;

            while((t1.LowPart - t0.LowPart) < 1000)
            {
                QueryPerformanceCounter(&t1);
                MyReadTimeStampCounter(&stamp1);
            }
        }

        diffStamp = stamp1 - stamp0;
        diffTicks = t1.LowPart - t0.LowPart;
        diffTicks *= 100000;
        diffTicks = diffTicks / (perfFreq.LowPart / 10);

        return (float)((float)diffStamp / (float)diffTicks);
    }
    else return 0.0f;
}

// ---------------------------------------------------------------------------

void OpenProfiler()
{
    // load user variable
    Profiler = GetConfigInt(USER_PROFILE, USER_PROFILE_DEFAULT);

    // status window should be valid
    if(!IsWindow(wnd.hStatusWindow)) Profiler = FALSE;

    // high-precision timer should exist
    LARGE_INTEGER freq;
    if(!QueryPerformanceFrequency(&freq)) Profiler = FALSE;

    // reset counters
    if(Profiler)
    {
        cpuTime = gfxTime = sfxTime = padTime = dvdTime = idleTime = 0;
        ONE_SECOND = (s64)((f64)GetClockSpeed() * (f64)1000000.0);
        MyReadTimeStampCounter(&startTime);
        MyReadTimeStampCounter(&fpsTime);
        MyReadTimeStampCounter(&mipsTime);
    }

    checkTime = GetTickCount();
}

// update after every fifo call
void UpdateProfiler()
{
    if(Profiler)
    {
        char buf[128], mips[128];

        if((GetTickCount() - checkTime) < 1000) return;
        checkTime = GetTickCount();
    
        // measure time
        MyReadTimeStampCounter(&stopTime);
        s64 total = stopTime - startTime;

        cpuTime = total - gfxTime - sfxTime - padTime - dvdTime;
        s64 cur; MyReadTimeStampCounter(&cur);
        s64 diff = cur - startTime;

        // calculate how long we can be in idle state (VSYNC, as in real)
        // current emulation speed is not allowing to waste any time
        idleTime = 0;
/*/
        if(diff < ONE_SECOND / 60)
        {
            idleTime = (ONE_SECOND / 60) - diff;
            total += idleTime;
        }
/*/

        // frames per second
        MyReadTimeStampCounter(&cur);
        diff = cur - fpsTime;
        if(diff >= ONE_SECOND)
        {
            sprintf(buf, "FPS:%u", vi.frames);
            vi.frames = 0;
            SetStatusText(STATUS_FPS, buf);
            MyReadTimeStampCounter(&fpsTime);
        }

        // calculate MIPS
        MyReadTimeStampCounter(&cur);
        diff = cur - mipsTime;
        if(cur >= ONE_SECOND)
        {
            sprintf(mips, "%.1f", (f32)cpu.ops / 1000000.0f);
            cpu.ops = 0;
            MyReadTimeStampCounter(&mipsTime);
        }
        else
        {
            char * st = GetStatusText(STATUS_PROGRESS);
            sscanf(st, "mips:%s ", mips);
        }

        // update status bar
        {
            //sprintf(buf, "mips:%s  core:%-2.1f  fifo:%-2.1f  sound:%-2.1f  input:%-2.1f  dvd:%-2.1f  idle:%-2.1f", 
            //sprintf(buf, "mips:%s  core:%-2.1f  gfx:%-2.1f  snd:%-2.1f  pad:%-2.1f  dvd:%-2.1f  idle:%-2.1f",
            sprintf(buf, "mips:%s  core:%-2.1f  video:%-2.1f  sound:%-2.1f  input:%-2.1f  dvd:%-2.1f  idle:%-2.1f", 
                mips,
                (f64)cpuTime * 100 / (f64)total,
                (f64)gfxTime * 100 / (f64)total,
                (f64)sfxTime * 100 / (f64)total,
                (f64)padTime * 100 / (f64)total,
                (f64)dvdTime * 100 / (f64)total,
                (f64)idleTime* 100 / (f64)total
            );
            SetStatusText(STATUS_PROGRESS, buf);
        }

        // reset counters
        cpuTime = gfxTime = sfxTime = padTime = dvdTime = idleTime = 0;
        MyReadTimeStampCounter(&startTime);
    }
}

// ---------------------------------------------------------------------------

// profilers set

void BeginProfileGfx() { if(Profiler) MyReadTimeStampCounter(&gfxStartTime); }
void EndProfileGfx()   { if(Profiler) { MyReadTimeStampCounter(&gfxStopTime);
                         gfxTime += gfxStopTime - gfxStartTime; } }

void BeginProfileSfx() { if(Profiler) MyReadTimeStampCounter(&sfxStartTime); }
void EndProfileSfx()   { if(Profiler) { MyReadTimeStampCounter(&sfxStopTime);
                         sfxTime += sfxStopTime - sfxStartTime; } }

void BeginProfilePAD() { if(Profiler) MyReadTimeStampCounter(&padStartTime); }
void EndProfilePAD()   { if(Profiler) { MyReadTimeStampCounter(&padStopTime);
                         padTime += padStopTime - padStartTime; } }

void BeginProfileDVD() { if(Profiler) MyReadTimeStampCounter(&dvdStartTime); }
void EndProfileDVD()   { if(Profiler) { MyReadTimeStampCounter(&dvdStopTime);
                         dvdTime += dvdStopTime - dvdStartTime; } }
