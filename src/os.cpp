// high level initialization code
#include "pch.h"

using namespace Debug;

HLEControl hle;

// ---------------------------------------------------------------------------

void os_ignore() { Report(Channel::HLE, "High level ignore (pc: %08X, %s)\n", Core->regs.pc, SYMName(Core->regs.pc)); }
void os_ret0() { Core->regs.gpr[3] = 0; }
void os_ret1() { Core->regs.gpr[3] = 1; }
void os_trap() { Core->regs.pc = Core->regs.spr[(int)Gekko::SPR::LR] - 4; Halt("High level trap (pc: %08X)!\n", Core->regs.pc); }

// HLE Ignore (you know what are you doing!)
static const char* osignore[] = {
	// video
	//"VIWaitForRetrace"          ,

	// Terminator
	"HLE_IGNORE",
	NULL
};

// HLE which return 0 as result
static const char* osret0[] = {

	// Terminator
	"HLE_RETURN0",
	NULL
};

// HLE which return 1 as result
static const char* osret1[] = {

	// Terminator
	"HLE_RETURN1",
	NULL
};

// HLE Traps (calls, which can cause unpredictable situation)
static const char* ostraps[] = {

	// Terminator
	"HLE_TRAP",
	NULL
};

// HLE Calls
static struct OSCalls
{
	char* name;
	void    (*call)();
} oscalls[] = {

	/*/

		// Interrupt handling
		{ "OSDisableInterrupts"     , OSDisableInterrupts       },
		{ "OSEnableInterrupts"      , OSEnableInterrupts        },
		{ "OSRestoreInterrupts"     , OSRestoreInterrupts       },

		// Context API
		// its working, but we need better recognition for OSLoadContext
		// minimal set: OSSaveContext, OSLoadContext, __OSContextInit.
		{ "OSSetCurrentContext"     , OSSetCurrentContext       },
		{ "OSGetCurrentContext"     , OSGetCurrentContext       },
		{ "OSSaveContext"           , OSSaveContext             },
		//{ "OSLoadContext"           , OSLoadContext             },
		{ "OSClearContext"          , OSClearContext            },
		{ "OSInitContext"           , OSInitContext             },
		{ "OSLoadFPUContext"        , OSLoadFPUContext          },
		{ "OSSaveFPUContext"        , OSSaveFPUContext          },
		{ "OSFillFPUContext"        , OSFillFPUContext          },
		{ "__OSContextInit"         , __OSContextInit           },

		// Std C
		{ "memset"                  , HLE_memset                },
		{ "memcpy"                  , HLE_memcpy                },

		{ "cos"                     , HLE_cos                   },
		{ "sin"                     , HLE_sin                   },
		{ "modf"                    , HLE_modf                  },
		{ "frexp"                   , HLE_frexp                 },
		{ "ldexp"                   , HLE_ldexp                 },
		{ "floor"                   , HLE_floor                 },
		{ "ceil"                    , HLE_ceil                  },
	/*/

	// Terminator
	{ NULL                      , NULL                      }
};

// ---------------------------------------------------------------------------

// wrapper
void HLESetCall(const char* name, void (*call)())
{
	SYMSetHighlevel(name, call);
}

void HLEInit()
{
	JDI::Hub.AddNode(HLE_JDI_JSON, HLE::JdiReflector);
}

void HLEShutdown()
{
	JDI::Hub.RemoveNode(HLE_JDI_JSON);
}

void HLEOpen()
{
	Report(Channel::Info,
		"-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
		"Highlevel Initialization.\n"
		"-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
	);

	// set high level calls
	int32_t n = 0;
	while (osignore[n])
	{
		HLESetCall(osignore[n++], os_ignore);
	} n = 0;
	while (osret0[n])
	{
		HLESetCall(osret0[n++], os_ret0);
	} n = 0;
	while (osret1[n])
	{
		HLESetCall(osret1[n++], os_ret1);
	} n = 0;
	while (ostraps[n])
	{
		HLESetCall(ostraps[n++], os_trap);
	} n = 0;
	while (oscalls[n].name)
	{
		HLESetCall(oscalls[n].name, oscalls[n].call);
		n++;
	}

	// Geometry library
	//MTXOpen();
}

void HLEClose()
{
	SYMKill();
}

void HLEExecuteCallback(uint32_t entryPoint)
{
	uint32_t old = Core->regs.spr[(int)Gekko::SPR::LR];
	Core->regs.pc = entryPoint;
	Core->regs.spr[(int)Gekko::SPR::LR] = 0;
	while (Core->regs.pc) Core->Step();
	Core->regs.pc = Core->regs.spr[(int)Gekko::SPR::LR] = old;
}



#define PARAM(n)    Core->regs.gpr[3+n]
#define RET_VAL     Core->regs.gpr[3]
#define SWAP        _BYTESWAP_UINT32
#define FPRDBL(n)     Core->regs.fpr[n].dbl

// fast longlong swap, invented by org
static void swap_double(void* srcPtr)
{
	uint8_t* src = (uint8_t*)srcPtr;
	uint8_t t;

	for (int i = 0; i < 4; i++)
	{
		t = src[7 - i];
		src[7 - i] = src[i];
		src[i] = t;
	}
}

/* ---------------------------------------------------------------------------
	Memory operations
--------------------------------------------------------------------------- */

// void *memcpy( void *dest, const void *src, size_t count );
void HLE_memcpy()
{
	int WIMG;
	uint32_t eaDest = PARAM(0), eaSrc = PARAM(1), cnt = PARAM(2);
	uint32_t paDest = Core->EffectiveToPhysical(eaDest, Gekko::MmuAccess::Read, WIMG);
	uint32_t paSrc = Core->EffectiveToPhysical(eaSrc, Gekko::MmuAccess::Read, WIMG);

	assert( paDest != Gekko::BadAddress);
	assert( paSrc != Gekko::BadAddress);
	assert( (paDest + cnt) < RAMSIZE);
	assert( (paSrc + cnt) < RAMSIZE);

//  DBReport( GREEN "memcpy(0x%08X, 0x%08X, %i(%s))\n", 
//            eaDest, eaSrc, cnt, FileSmartSize(cnt) );

	memcpy(&mi.ram[paDest], &mi.ram[paSrc], cnt);
}

// void *memset( void *dest, int c, size_t count );
void HLE_memset()
{
	int WIMG;
	uint32_t eaDest = PARAM(0), c = PARAM(1), cnt = PARAM(2);
	uint32_t paDest = Core->EffectiveToPhysical(eaDest, Gekko::MmuAccess::Read, WIMG);

	assert(paDest != Gekko::BadAddress);
	assert( (paDest + cnt) < RAMSIZE);

//  DBReport( GREEN "memcpy(0x%08X, %i(%c), %i(%s))\n", 
//            eaDest, c, cnt, FileSmartSize(cnt) );

	memset(&mi.ram[paDest], c, cnt);
}

/* ---------------------------------------------------------------------------
	String operations
--------------------------------------------------------------------------- */


/* ---------------------------------------------------------------------------
	FP Math
--------------------------------------------------------------------------- */

// double sin(double x)
void HLE_sin()
{
	FPRDBL(1) = sin(FPRDBL(1));
}

// double cos(double x)
void HLE_cos()
{
	FPRDBL(1) = cos(FPRDBL(1));
}

// double modf(double x, double * intptr)
void HLE_modf()
{
	int WIMG;
	double * intptr = (double *)(&mi.ram[Core->EffectiveToPhysical(PARAM(0), Gekko::MmuAccess::Read, WIMG)]);
	
	FPRDBL(1) = modf(FPRDBL(1), intptr);
	swap_double(intptr);
}

// double frexp(double x, int * expptr)
void HLE_frexp()
{
	int WIMG;
	uint32_t * expptr = (uint32_t *)(&mi.ram[Core->EffectiveToPhysical(PARAM(0), Gekko::MmuAccess::Read, WIMG)]);
	
	FPRDBL(1) = frexp(FPRDBL(1), (int *)expptr);
	*expptr = SWAP(*expptr);
}

// double ldexp(double x, int exp)
void HLE_ldexp()
{
	FPRDBL(1) = ldexp(FPRDBL(1), (int)PARAM(0));
}

// double floor(double x)
void HLE_floor()
{
	FPRDBL(1) = floor(FPRDBL(1));
}

// double ceil(double x)
void HLE_ceil()
{
	FPRDBL(1) = ceil(FPRDBL(1));
}



namespace HLE
{

	// Convert GC time to human-usable time string;
	// Example output: "30 Jun 2004 3:06:14:127"
	std::string OSTimeFormat(uint64_t tbr, bool noDate)
	{
		char timeStr[0x100];

		static const char* mnstr[12] = {
			"Jan", "Feb", "Mar", "Apr",
			"May", "Jun", "Jul", "Aug",
			"Sep", "Oct", "Nov", "Dec"
		};

		static const int dayMon[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		static const int dayMonLeap[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		#define IsLeapYear(y)  ((y % 1000) == 0 || (y % 4) == 0)

		int64_t ticksPerMs = Core->OneSecond() / 1000;
		int64_t ms = tbr / ticksPerMs;
		int64_t msPerDay = 24 * 60 * 60 * 1000;
		int64_t days = ms / msPerDay;
		int64_t msDay = ms - days * msPerDay;

		// Hour, Minute, Second

		int h = (int)(msDay / (60 * 60 * 1000));
		msDay -= h * (60 * 60 * 1000);
		int m = (int)(msDay / (60 * 1000));
		msDay -= m * (60 * 1000);
		int s = (int)(msDay / 1000);
		msDay -= s * 1000;

		int year = 2000;
		int mon = 0;

		// Year, Month, Day

		while (days >= (IsLeapYear(year) ? 366 : 365))
		{
			days -= IsLeapYear(year) ? 366 : 365;
			year++;
		}

		while (days >= (IsLeapYear(year) ? dayMonLeap[mon] : dayMon[mon]))
		{
			days -= IsLeapYear(year) ? dayMonLeap[mon] : dayMon[mon];
			mon++;
		}

		int day = (int)(days + 1);

		if (noDate)
		{
			sprintf(timeStr, "%02i:%02i:%02i:%03i",
				h, m, s, (int)msDay);
		}
		else
		{
			sprintf(timeStr, "%i %s %i %02i:%02i:%02i:%03i",
				day, mnstr[mon], year,
				h, m, s, (int)msDay);
		}

		return timeStr;
	}

}


// Dump DolphinOS threads.

// Details on the DolphinOS threads can be found in \Docs\RE\thread.txt. Fairly simple and clean design.
// We catch on to the list of active threads (__OSLinkActive) and display them in turn. You can use the DumpContext command to display context.


namespace HLE
{

	static bool LoadOsThread(uint32_t ea, OSThread* thread)
	{
		int WIMG;

		// Translate effective address

		uint32_t threadPa = Core->EffectiveToPhysical(ea, Gekko::MmuAccess::Read, WIMG);
		if (threadPa == Gekko::BadAddress)
		{
			Report(Channel::Norm, "Invalid thread effective address: 0x%08X\n", ea);
			return false;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(threadPa, sizeof(OSThread));

		if (ptr == nullptr)
		{
			Report(Channel::Norm, "Invalid thread physical address: 0x%08X\n", threadPa);
			return false;
		}

		// Load thread struct and swap values

		*thread = *(OSThread*)ptr;

		thread->state = _BYTESWAP_UINT16(thread->state);
		thread->attr = _BYTESWAP_UINT16(thread->attr);
		thread->suspend = _BYTESWAP_UINT32(thread->suspend);
		thread->priority = _BYTESWAP_UINT32(thread->priority);
		thread->base = _BYTESWAP_UINT32(thread->base);
		thread->val = _BYTESWAP_UINT32(thread->val);

		thread->linkActive.next = _BYTESWAP_UINT32(thread->linkActive.next);
		thread->linkActive.prev = _BYTESWAP_UINT32(thread->linkActive.prev);

		thread->stackBase = _BYTESWAP_UINT32(thread->stackBase);
		thread->stackEnd = _BYTESWAP_UINT32(thread->stackEnd);

		// No need for other properties

		return true;
	}

	static void DumpOsThread(size_t count, OSThread* thread, uint32_t threadEa)
	{
		Report(Channel::Norm, "Thread %zi, context: 0x%08X:\n", count, threadEa);
		Report(Channel::Norm, "state: 0x%04X, attr: 0x%04X\n", thread->state, thread->attr);
		Report(Channel::Norm, "suspend: %i, priority: 0x%08X, base: 0x%08X, val: 0x%08X\n", (int)thread->suspend, thread->priority, thread->base, thread->val);
	}

	Json::Value* DumpDolphinOsThreads(bool displayOnScreen)
	{
		int WIMG;

		// Get pointer to __OSLinkActive

		uint32_t linkActiveEffectiveAddr = OS_LINK_ACTIVE;

		uint32_t linkActivePa = Core->EffectiveToPhysical(linkActiveEffectiveAddr, Gekko::MmuAccess::Read, WIMG);
		if (linkActivePa == Gekko::BadAddress)
		{
			if (displayOnScreen)
			{
				Report(Channel::Norm, "Invalid active threads link effective address: 0x%08X\n", linkActiveEffectiveAddr);
			}
			return nullptr;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(linkActivePa, sizeof(OSThreadLink));

		if (ptr == nullptr)
		{
			if (displayOnScreen)
			{
				Report(Channel::Norm, "Invalid active threads link physical address: 0x%08X\n", linkActivePa);
			}
			return nullptr;
		}

		OSThreadLink linkActive = *(OSThreadLink*)ptr;

		linkActive.next = _BYTESWAP_UINT32(linkActive.next);
		linkActive.prev = _BYTESWAP_UINT32(linkActive.prev);

		// Walk active threads

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		if (displayOnScreen)
		{
			Report(Channel::Norm, "Dumping active DolphinOS threads:\n\n");
		}

		size_t activeThreadsCount = 0;
		uint32_t threadEa = linkActive.next;

		while (threadEa != 0)
		{
			OSThread thread = { 0 };

			if (!LoadOsThread(threadEa, &thread))
				break;

			if (displayOnScreen)
			{
				DumpOsThread(activeThreadsCount, &thread, threadEa);
			}

			threadEa = thread.linkActive.next;
			activeThreadsCount++;

			output->AddUInt32(nullptr, threadEa);

			if (displayOnScreen)
			{
				Report(Channel::Norm, "\n");
			}
		}

		if (displayOnScreen)
		{
			Report(Channel::Norm, "Active threads: %zi. Use DumpContext command to dump thread context.\n", activeThreadsCount);
		}

		return output;
	}

	Json::Value* DumpDolphinOsContext(uint32_t effectiveAddr, bool displayOnScreen)
	{
		int WIMG;

		// Get context pointer

		uint32_t physAddr = Core->EffectiveToPhysical(effectiveAddr, Gekko::MmuAccess::Read, WIMG);

		if (physAddr == Gekko::BadAddress)
		{
			if (displayOnScreen)
			{
				Report(Channel::Norm, "Invalid context effective address: 0x%08X\n", effectiveAddr);
			}
			return nullptr;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(physAddr, sizeof(OSContext));

		if (ptr == nullptr)
		{
			if (displayOnScreen)
			{
				Report(Channel::Norm, "Invalid context physical address: 0x%08X\n", physAddr);
			}
			return nullptr;
		}

		// Copyout context and swap values

		OSContext context = *(OSContext*)ptr;

		for (int i = 0; i < 32; i++)
		{
			context.gpr[i] = _BYTESWAP_UINT32(context.gpr[i]);
			context.fprAsUint[i] = _BYTESWAP_UINT64(context.fprAsUint[i]);
			context.psrAsUint[i] = _BYTESWAP_UINT64(context.psrAsUint[i]);
		}

		for (int i = 0; i < 8; i++)
		{
			context.gqr[i] = _BYTESWAP_UINT32(context.gqr[i]);
		}

		context.cr = _BYTESWAP_UINT32(context.cr);
		context.lr = _BYTESWAP_UINT32(context.lr);
		context.ctr = _BYTESWAP_UINT32(context.ctr);
		context.xer = _BYTESWAP_UINT32(context.xer);

		context.fpscr = _BYTESWAP_UINT32(context.fpscr);

		context.srr[0] = _BYTESWAP_UINT32(context.srr[0]);
		context.srr[1] = _BYTESWAP_UINT32(context.srr[1]);

		context.mode = _BYTESWAP_UINT16(context.mode);
		context.state = _BYTESWAP_UINT16(context.state);

		// Dump contents

		if (displayOnScreen)
		{
			for (int i = 0; i < 32; i++)
			{
				Report(Channel::Norm, "gpr[%i] = 0x%08X\n", i, context.gpr[i]);
			}

			for (int i = 0; i < 32; i++)
			{
				Report(Channel::Norm, "fpr[%i] = %f (0x%llx)\n", i, context.fpr[i], context.fprAsUint[i]);
			}

			for (int i = 0; i < 32; i++)
			{
				Report(Channel::Norm, "psr[%i] = %f (0x%llx)\n", i, context.psr[i], context.psrAsUint[i]);
			}

			for (int i = 0; i < 8; i++)
			{
				Report(Channel::Norm, "gqr[%i] = 0x%08X\n", i, context.gqr[i]);
			}

			Report(Channel::Norm, "cr = 0x%08X\n", context.cr);
			Report(Channel::Norm, "lr = 0x%08X\n", context.lr);
			Report(Channel::Norm, "ctr = 0x%08X\n", context.ctr);
			Report(Channel::Norm, "xer = 0x%08X\n", context.xer);

			Report(Channel::Norm, "fpscr = 0x%08X\n", context.fpscr);

			Report(Channel::Norm, "srr[0] = 0x%08X\n", context.srr[0]);
			Report(Channel::Norm, "srr[1] = 0x%08X\n", context.srr[1]);

			Report(Channel::Norm, "mode = 0x%02X\n", (uint8_t)context.mode);
			Report(Channel::Norm, "state = 0x%02X\n", (uint8_t)context.state);
		}

		// Serialize

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		for (int i = 0; i < 32; i++)
		{
			output->AddUInt32(nullptr, context.gpr[i]);
		}

		for (int i = 0; i < 32; i++)
		{
			output->AddFloat(nullptr, (float)context.fpr[i]);
		}

		for (int i = 0; i < 32; i++)
		{
			output->AddFloat(nullptr, (float)context.psr[i]);
		}

		for (int i = 0; i < 8; i++)
		{
			output->AddUInt32(nullptr, context.gqr[i]);
		}

		output->AddUInt32(nullptr, context.cr);
		output->AddUInt32(nullptr, context.lr);
		output->AddUInt32(nullptr, context.ctr);
		output->AddUInt32(nullptr, context.xer);

		output->AddUInt32(nullptr, context.fpscr);

		output->AddUInt32(nullptr, context.srr[0]);
		output->AddUInt32(nullptr, context.srr[1]);

		output->AddUInt16(nullptr, context.mode);
		output->AddUInt16(nullptr, context.state);

		return output;
	}

}




// DolphinSDK Vector/Matrix math library emulation.

// The modern x64 SSE optimizer should optimize such code pretty well, without any ritual squats.

// pre-swapped 1.0f and 0.0f
#define ONE         0x803f
#define ZERO        0

typedef struct Matrix
{
	uint32_t     data[3][4];
} Matrix, *MatrixPtr;
typedef struct MatrixF
{
	float     data[3][4];
} MatrixF, *MatrixFPtr;

#define MTX(mx)  mx->data

static void print_mtx(MatrixPtr ptr)
{
	MatrixFPtr m = (MatrixFPtr)ptr;

	Gekko::GekkoCore::SwapArea((uint32_t *)ptr, 3*4*4);
	Report(Channel::Norm,
		"%f %f %f %f\n"
		"%f %f %f %f\n"
		"%f %f %f %f\n",

		MTX(m)[0][0], MTX(m)[0][1], MTX(m)[0][2], MTX(m)[0][3],
		MTX(m)[1][0], MTX(m)[1][1], MTX(m)[1][2], MTX(m)[1][3],
		MTX(m)[2][0], MTX(m)[2][1], MTX(m)[2][2], MTX(m)[2][3] );
	Gekko::GekkoCore::SwapArea((uint32_t *)ptr, 3*4*4);
}

/* ---------------------------------------------------------------------------
	Init layer
--------------------------------------------------------------------------- */

void MTXOpen()
{
	bool flag = false;//GetConfigInt(USER_HLE_MTX, USER_HLE_MTX_DEFAULT);
	if(flag == false) return;

	Report( Channel::HLE, "Geometry library install\n");

	HLESetCall("C_MTXIdentity",             C_MTXIdentity);
	HLESetCall("PSMTXIdentity",             C_MTXIdentity);
	HLESetCall("C_MTXCopy",                 C_MTXCopy);
	HLESetCall("PSMTXCopy",                 C_MTXCopy);

	HLESetCall("C_MTXConcat",               C_MTXConcat);
	HLESetCall("PSMTXConcat",               C_MTXConcat);
	HLESetCall("C_MTXTranspose",            C_MTXTranspose);
	HLESetCall("PSMTXTranspose",            C_MTXTranspose);

	Report(Channel::Norm, "\n");
}

/* ---------------------------------------------------------------------------
	General stuff
--------------------------------------------------------------------------- */

static Matrix tmpMatrix[4];

void C_MTXIdentity(void)
{
	MatrixPtr m = (MatrixPtr)(&mi.ram[PARAM(0) & RAMMASK]);

	MTX(m)[0][0] = ONE;  MTX(m)[0][1] = ZERO; MTX(m)[0][2] = ZERO; MTX(m)[0][3] = ZERO;
	MTX(m)[1][0] = ZERO; MTX(m)[1][1] = ONE;  MTX(m)[1][2] = ZERO; MTX(m)[1][3] = ZERO;
	MTX(m)[2][0] = ZERO; MTX(m)[2][1] = ZERO; MTX(m)[2][2] = ONE;  MTX(m)[2][3] = ZERO;
}

void C_MTXCopy(void)
{
	MatrixPtr src = (MatrixPtr)(&mi.ram[PARAM(0) & RAMMASK]);
	MatrixPtr dst = (MatrixPtr)(&mi.ram[PARAM(1) & RAMMASK]);

	if(src == dst) return;

	MTX(dst)[0][0] = MTX(src)[0][0]; MTX(dst)[0][1] = MTX(src)[0][1]; MTX(dst)[0][2] = MTX(src)[0][2]; MTX(dst)[0][3] = MTX(src)[0][3];
	MTX(dst)[1][0] = MTX(src)[1][0]; MTX(dst)[1][1] = MTX(src)[1][1]; MTX(dst)[1][2] = MTX(src)[1][2]; MTX(dst)[1][3] = MTX(src)[1][3];
	MTX(dst)[2][0] = MTX(src)[2][0]; MTX(dst)[2][1] = MTX(src)[2][1]; MTX(dst)[2][2] = MTX(src)[2][2]; MTX(dst)[2][3] = MTX(src)[2][3];

	//print_mtx((MatrixPtr)src, "src C");
	//print_mtx((MatrixPtr)dst, "dst C");
}

void C_MTXConcat(void)
{
	MatrixFPtr a = (MatrixFPtr)(&mi.ram[PARAM(0) & RAMMASK]);
	MatrixFPtr b = (MatrixFPtr)(&mi.ram[PARAM(1) & RAMMASK]);
	MatrixFPtr axb = (MatrixFPtr)(&mi.ram[PARAM(2) & RAMMASK]);
	MatrixFPtr t = (MatrixFPtr)(&tmpMatrix[0]), m;

	if( (axb == a) || (axb == b) ) m = t;
	else m = axb;

	//print_mtx((MatrixPtr)a, "a C");
	//print_mtx((MatrixPtr)b, "b C");

	Gekko::GekkoCore::SwapArea((uint32_t *)a, 3*4*4);
	Gekko::GekkoCore::SwapArea((uint32_t *)b, 3*4*4);

	// m = a x b
	MTX(m)[0][0] = MTX(a)[0][0]*MTX(b)[0][0] + MTX(a)[0][1]*MTX(b)[1][0] + MTX(a)[0][2]*MTX(b)[2][0];
	MTX(m)[0][1] = MTX(a)[0][0]*MTX(b)[0][1] + MTX(a)[0][1]*MTX(b)[1][1] + MTX(a)[0][2]*MTX(b)[2][1];
	MTX(m)[0][2] = MTX(a)[0][0]*MTX(b)[0][2] + MTX(a)[0][1]*MTX(b)[1][2] + MTX(a)[0][2]*MTX(b)[2][2];
	MTX(m)[0][3] = MTX(a)[0][0]*MTX(b)[0][3] + MTX(a)[0][1]*MTX(b)[1][3] + MTX(a)[0][2]*MTX(b)[2][3] + MTX(a)[0][3];

	MTX(m)[1][0] = MTX(a)[1][0]*MTX(b)[0][0] + MTX(a)[1][1]*MTX(b)[1][0] + MTX(a)[1][2]*MTX(b)[2][0];
	MTX(m)[1][1] = MTX(a)[1][0]*MTX(b)[0][1] + MTX(a)[1][1]*MTX(b)[1][1] + MTX(a)[1][2]*MTX(b)[2][1];
	MTX(m)[1][2] = MTX(a)[1][0]*MTX(b)[0][2] + MTX(a)[1][1]*MTX(b)[1][2] + MTX(a)[1][2]*MTX(b)[2][2];
	MTX(m)[1][3] = MTX(a)[1][0]*MTX(b)[0][3] + MTX(a)[1][1]*MTX(b)[1][3] + MTX(a)[1][2]*MTX(b)[2][3] + MTX(a)[1][3];

	MTX(m)[2][0] = MTX(a)[2][0]*MTX(b)[0][0] + MTX(a)[2][1]*MTX(b)[1][0] + MTX(a)[2][2]*MTX(b)[2][0];
	MTX(m)[2][1] = MTX(a)[2][0]*MTX(b)[0][1] + MTX(a)[2][1]*MTX(b)[1][1] + MTX(a)[2][2]*MTX(b)[2][1];
	MTX(m)[2][2] = MTX(a)[2][0]*MTX(b)[0][2] + MTX(a)[2][1]*MTX(b)[1][2] + MTX(a)[2][2]*MTX(b)[2][2];
	MTX(m)[2][3] = MTX(a)[2][0]*MTX(b)[0][3] + MTX(a)[2][1]*MTX(b)[1][3] + MTX(a)[2][2]*MTX(b)[2][3] + MTX(a)[2][3];

	// restore A and B
	Gekko::GekkoCore::SwapArea((uint32_t *)a, 3*4*4);
	Gekko::GekkoCore::SwapArea((uint32_t *)b, 3*4*4);
	Gekko::GekkoCore::SwapArea((uint32_t *)m, 3*4*4);

	//print_mtx((MatrixPtr)m, "m C");

	// overwrite a (b)
	if(m == t)
	{
		MatrixPtr src = (MatrixPtr)t, dst = (MatrixPtr)axb;
		if(src != dst)
		{
			MTX(dst)[0][0] = MTX(src)[0][0]; MTX(dst)[0][1] = MTX(src)[0][1]; MTX(dst)[0][2] = MTX(src)[0][2]; MTX(dst)[0][3] = MTX(src)[0][3];
			MTX(dst)[1][0] = MTX(src)[1][0]; MTX(dst)[1][1] = MTX(src)[1][1]; MTX(dst)[1][2] = MTX(src)[1][2]; MTX(dst)[1][3] = MTX(src)[1][3];
			MTX(dst)[2][0] = MTX(src)[2][0]; MTX(dst)[2][1] = MTX(src)[2][1]; MTX(dst)[2][2] = MTX(src)[2][2]; MTX(dst)[2][3] = MTX(src)[2][3];
		}
	}
}

void C_MTXTranspose(void)
{
	MatrixPtr src = (MatrixPtr)(&mi.ram[PARAM(0) & RAMMASK]);
	MatrixPtr xPose = (MatrixPtr)(&mi.ram[PARAM(1) & RAMMASK]);
	MatrixPtr t = (&tmpMatrix[0]), m;

	if(src == xPose) m = t;
	else m = xPose;

	MTX(m)[0][0] = MTX(src)[0][0]; MTX(m)[0][1] = MTX(src)[1][0]; MTX(m)[0][2] = MTX(src)[2][0]; MTX(m)[0][3] = ZERO;
	MTX(m)[1][0] = MTX(src)[0][1]; MTX(m)[1][1] = MTX(src)[1][1]; MTX(m)[1][2] = MTX(src)[2][1]; MTX(m)[1][3] = ZERO;
	MTX(m)[2][0] = MTX(src)[0][2]; MTX(m)[2][1] = MTX(src)[1][2]; MTX(m)[2][2] = MTX(src)[2][2]; MTX(m)[2][3] = ZERO;

	//print_mtx((MatrixPtr)m, "m C");

	// overwrite
	if(m == t)
	{
		MatrixPtr src = t, dst = xPose;
		if(src != dst)
		{
			MTX(dst)[0][0] = MTX(src)[0][0]; MTX(dst)[0][1] = MTX(src)[0][1]; MTX(dst)[0][2] = MTX(src)[0][2]; MTX(dst)[0][3] = MTX(src)[0][3];
			MTX(dst)[1][0] = MTX(src)[1][0]; MTX(dst)[1][1] = MTX(src)[1][1]; MTX(dst)[1][2] = MTX(src)[1][2]; MTX(dst)[1][3] = MTX(src)[1][3];
			MTX(dst)[2][0] = MTX(src)[2][0]; MTX(dst)[2][1] = MTX(src)[2][1]; MTX(dst)[2][2] = MTX(src)[2][2]; MTX(dst)[2][3] = MTX(src)[2][3];
		}
	}
}

void C_MTXInverse(void)
{
}

void C_MTXInvXpose(void)
{
}



// high level Dolphin OS (experimental)

// internal OS vars
static  uint32_t     __OSPhysicalContext;    // OS_PHYSICAL_CONTEXT
static  uint32_t     __OSCurrentContext;     // OS_CURRENT_CONTEXT

static  uint32_t     __OSDefaultThread;      // OS_DEFAULT_THREAD

/* ---------------------------------------------------------------------------
	Context API, based on Dolphin OS reversing of OSContext module
--------------------------------------------------------------------------- */

// IMPORTANT : FPRs are ALWAYS saved, because FP Unavail handler is not used

// stack operations are not emulated, because they are simple

void OSSetCurrentContext(void)
{
	__OSCurrentContext  = PARAM(0);
	__OSPhysicalContext = __OSCurrentContext & RAMMASK; // simple translation
	Core->WriteWord(OS_CURRENT_CONTEXT, __OSCurrentContext);
	Core->WriteWord(OS_PHYSICAL_CONTEXT, __OSPhysicalContext);

	OSContext *c = (OSContext *)(&mi.ram[__OSPhysicalContext]);

	if(__OSCurrentContext == __OSDefaultThread/*context*/)
	{
		c->srr[1] |= SWAP(MSR_FP);
	}
	else
	{
		// floating point regs are always available!
		//c->srr[1] &= ~SWAP(MSR_FP);
		//MSR &= ~MSR_FP;

		c->srr[1] |= SWAP(MSR_FP);
		Core->regs.msr |= MSR_FP;
	}

	Core->regs.msr |= MSR_RI;
}

void OSGetCurrentContext(void)
{
	RET_VAL = __OSCurrentContext;
}

void OSSaveContext(void)
{
	int i;

	OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);

	// always save FP/PS context
	OSSaveFPUContext();

	// save gprs
	for(i=13; i<32; i++)
		c->gpr[i] = SWAP(Core->regs.gpr[i]);

	// save gqrs 1..7 (GRQ0 is always 0)
	for(i=1; i<8; i++)
		c->gqr[i] = SWAP(Core->regs.spr[(int)Gekko::SPR::GQRs + i]);

	// misc regs
	c->cr = SWAP(Core->regs.cr);
	c->lr = SWAP(Core->regs.spr[(int)Gekko::SPR::LR]);
	c->ctr = SWAP(Core->regs.spr[(int)Gekko::SPR::CTR]);
	c->xer = SWAP(Core->regs.spr[(int)Gekko::SPR::XER]);
	c->srr[0] = c->lr;
	c->srr[1] = SWAP(Core->regs.msr);

	c->gpr[1] = SWAP(Core->regs.gpr[1]);
	c->gpr[2] = SWAP(Core->regs.gpr[2]);
	c->gpr[3] = SWAP(Core->regs.gpr[0] = 1);

	RET_VAL = 0;
	// usual blr
}

// OSLoadContext return is patched as RFI (not usual BLR)
// see Symbols.cpp, SYMSetHighlevel, line 97
void OSLoadContext(void)
{
	OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);

	// thread switch on OSDisableInterrupts is omitted, because
	// interrupts are generated only at branch opcodes;
	// r0, r4, r5, r6 are garbed here ..

	// load gprs 0..2
	Core->regs.gpr[0] = SWAP(c->gpr[0]);
	Core->regs.gpr[1] = SWAP(c->gpr[1]);   // SP
	Core->regs.gpr[2] = SWAP(c->gpr[2]);   // SDA2
	
	// always load FP/PS context
	OSLoadFPUContext();

	// load gqrs 1..7 (GRQ0 is always 0)
	for(int i=1; i<8; i++)
		Core->regs.spr[(int)Gekko::SPR::GQRs + i] = SWAP(c->gqr[i]);

	// load other gprs
	uint16_t state = (c->state >> 8) | (c->state << 8);
	if(state & OS_CONTEXT_STATE_EXC)
	{
		state &= ~OS_CONTEXT_STATE_EXC;
		c->state = (state >> 8) | (state << 8);
		for(int i=5; i<32; i++)
			Core->regs.gpr[i] = SWAP(c->gpr[i]);
	}
	else
	{
		for(int i=13; i<32; i++)
			Core->regs.gpr[i] = SWAP(c->gpr[i]);
	}

	// misc regs
	Core->regs.cr  = SWAP(c->cr);
	Core->regs.spr[(int)Gekko::SPR::LR] = SWAP(c->lr);
	Core->regs.spr[(int)Gekko::SPR::CTR] = SWAP(c->ctr);
	Core->regs.spr[(int)Gekko::SPR::XER] = SWAP(c->xer);

	// set srr regs to update msr and pc
	Core->regs.spr[(int)Gekko::SPR::SRR0] = SWAP(c->srr[0]);
	Core->regs.spr[(int)Gekko::SPR::SRR1] = SWAP(c->srr[1]);

	Core->regs.gpr[3] = SWAP(c->gpr[3]);
	Core->regs.gpr[4] = SWAP(c->gpr[4]);
	// rfi will be called
}

void OSClearContext(void)
{
	OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);

	c->mode = 0;
	c->state = 0;

	if(PARAM(0) == __OSDefaultThread/*context*/) 
	{
		__OSDefaultThread = 0;
		Core->WriteWord(OS_DEFAULT_THREAD, __OSDefaultThread);
	}
}

void OSInitContext(void)
{
	int i;

	OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);

	c->srr[0] = SWAP(PARAM(1));
	c->gpr[1] = SWAP(PARAM(2));
	c->srr[1] = SWAP(MSR_EE | MSR_ME | MSR_IR | MSR_DR | MSR_RI);
	
	c->cr = 0;
	c->xer = 0;

	for(i=0; i<8; i++)
		c->gqr[i] = 0;

	OSClearContext();

	for(i=3; i<32; i++)
		c->gpr[i] = 0;
	c->gpr[2] = SWAP(Core->regs.gpr[2]);
	c->gpr[13] = SWAP(Core->regs.gpr[13]);
}

void OSLoadFPUContext(void)
{
	PARAM(1) = PARAM(0);
	OSContext * c = (OSContext *)(&mi.ram[PARAM(1) & RAMMASK]);

	//u16 state = (c->state >> 8) | (c->state << 8);
	//if(! (state & OS_CONTEXT_STATE_FPSAVED) )
	{
		Core->regs.fpscr = SWAP(c->fpscr);

		for(int i=0; i<32; i++)
		{
			if(Core->regs.spr[(int)Gekko::SPR::HID2] & HID2_PSE)
			{
				Core->regs.ps1[i].uval = *(uint64_t *)(&c->psr[i]);
				swap_double(&Core->regs.ps1[i].uval);
			}
			Core->regs.fpr[i].uval = *(uint64_t *)(&c->fpr[i]);
			swap_double(&Core->regs.fpr[i].uval);
		}
	}
}

void OSSaveFPUContext(void)
{
	PARAM(2) = PARAM(0);
	OSContext * c = (OSContext *)(&mi.ram[PARAM(2) & RAMMASK]);

	//c->state |= (OS_CONTEXT_STATE_FPSAVED >> 8) | (OS_CONTEXT_STATE_FPSAVED << 8);
	c->fpscr = SWAP(Core->regs.fpscr);

	for(int i=0; i<32; i++)
	{
		*(uint64_t *)(&c->fpr[i]) = Core->regs.fpr[i].uval;
		swap_double(&c->fpr[i]);
		if(Core->regs.spr[(int)Gekko::SPR::HID2] & HID2_PSE)
		{
			*(uint64_t *)(&c->psr[i]) = Core->regs.ps1[i].uval;
			swap_double(&c->psr[i]);
		}
	}
}

void OSFillFPUContext(void)
{
	OSContext * c = (OSContext *)(&mi.ram[PARAM(0) & RAMMASK]);
	
	Core->regs.msr |= MSR_FP;
	c->fpscr = SWAP(Core->regs.fpscr);

	for(int i=0; i<32; i++)
	{
		*(uint64_t *)(&c->fpr[i]) = Core->regs.fpr[i].uval;
		swap_double(&c->fpr[i]);
		if(Core->regs.spr[(int)Gekko::SPR::HID2] & HID2_PSE)
		{
			*(uint64_t *)(&c->psr[i]) = Core->regs.ps1[i].uval;
			swap_double(&c->psr[i]);
		}
	}
}

void __OSContextInit(void)
{
	Report(Channel::HLE, "HLE OS context driver installed.\n");
	Report(Channel::HLE, "Note: FP-Unavail is NOT used and FPRs are always saved.\n\n");

	__OSDefaultThread = 0;
	Core->WriteWord(OS_DEFAULT_THREAD, __OSDefaultThread);
	Core->regs.msr |= (MSR_FP | MSR_RI);
}

/* ---------------------------------------------------------------------------
	Interrupt handling
--------------------------------------------------------------------------- */

// called VERY often!
void OSDisableInterrupts(void)
{
	uint32_t prev = Core->regs.msr;
	Core->regs.msr &= ~MSR_EE;
	RET_VAL = (prev >> 15) & 1;
}

// this one is rare
void OSEnableInterrupts(void)
{
	uint32_t prev = Core->regs.msr;
	Core->regs.msr |= MSR_EE;
	RET_VAL = (prev >> 15) & 1;
}

// called VERY often!
void OSRestoreInterrupts(void)
{
	uint32_t prev = Core->regs.msr;
	if(PARAM(0)) Core->regs.msr |= MSR_EE;
	else Core->regs.msr &= ~MSR_EE;
	RET_VAL = (prev >> 15) & 1;
}
