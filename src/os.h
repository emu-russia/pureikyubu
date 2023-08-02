#pragma once


void    os_ignore();
void    os_ret0();
void    os_ret1();
void    os_trap();

// HLE state variables
struct HLEControl
{
    // current loaded map file
    wchar_t       mapfile[0x1000];
};

extern  HLEControl hle;

void    HLESetCall(const char* name, void (*call)());
void    HLEInit();
void    HLEShutdown();
void    HLEOpen();
void    HLEClose();
void    HLEExecuteCallback(uint32_t entryPoint);



/* ---------------------------------------------------------------------------
    Memory operations
--------------------------------------------------------------------------- */

void    HLE_memcpy();
void    HLE_memset();

/* ---------------------------------------------------------------------------
    String operations
--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
    FP Math
--------------------------------------------------------------------------- */

void    HLE_sin();
void    HLE_cos();
void    HLE_modf();
void    HLE_frexp();
void    HLE_ldexp();
void    HLE_floor();
void    HLE_ceil();



namespace HLE
{
    std::string OSTimeFormat(uint64_t tbr, bool noDate);
}



namespace HLE
{
    Json::Value* DumpDolphinOsThreads(bool displayOnScreen);

    Json::Value* DumpDolphinOsContext(uint32_t effectiveAddr, bool displayOnScreen);
}


void    os_ignore();
void    os_ret0();
void    os_ret1();
void    os_trap();

// HLE state variables
struct HLEControl
{
    // current loaded map file
    wchar_t       mapfile[0x1000];
};

extern  HLEControl hle;

void    HLESetCall(const char* name, void (*call)());
void    HLEInit();
void    HLEShutdown();
void    HLEOpen();
void    HLEClose();
void    HLEExecuteCallback(uint32_t entryPoint);



void    MTXOpen();

/* ---------------------------------------------------------------------------
    General stuff
--------------------------------------------------------------------------- */

/*/
void    MTXIdentity         ( Mtx m );
void    MTXCopy             ( Mtx src, Mtx dst );
void    MTXConcat           ( Mtx a, Mtx b, Mtx ab );
void    MTXTranspose        ( Mtx src, Mtx xPose );
u32     MTXInverse          ( Mtx src, Mtx inv );
u32     MTXInvXpose         ( Mtx src, Mtx invX );
/*/

// C version
void    C_MTXIdentity(void);
void    C_MTXCopy(void);
void    C_MTXConcat(void);
void    C_MTXTranspose(void);
void    C_MTXInverse(void);
void    C_MTXInvXpose(void);

/* ---------------------------------------------------------------------------
    Matrix-vector
--------------------------------------------------------------------------- */

/*/
void    MTXMultVec          ( Mtx m, VecPtr src, VecPtr dst );
void    MTXMultVecArray     ( Mtx m, VecPtr srcBase, VecPtr dstBase, u32 count );
void    MTXMultVecSR        ( Mtx m, VecPtr src, VecPtr dst );
void    MTXMultVecArraySR   ( Mtx m, VecPtr srcBase, VecPtr dstBase, u32 count );
/*/

/* ---------------------------------------------------------------------------
    Affine matrix math
--------------------------------------------------------------------------- */

/*/
void    MTXQuat             ( Mtx m, QuaternionPtr q );
void    MTXReflect          ( Mtx m, VecPtr p, VecPtr n );
void    MTXTrans            ( Mtx m, f32 xT, f32 yT, f32 zT );
void    MTXTransApply       ( Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT );
void    MTXScale            ( Mtx m, f32 xS, f32 yS, f32 zS );
void    MTXScaleApply       ( Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS );
void    MTXRotRad           ( Mtx m, char axis, f32 rad );
void    MTXRotTrig          ( Mtx m, char axis, f32 sinA, f32 cosA );
void    MTXRotAxisRad       ( Mtx m, VecPtr axis, f32 rad );
/*/

/* ---------------------------------------------------------------------------
    Look at utility
--------------------------------------------------------------------------- */

/*/
void    MTXLookAt           ( Mtx           m,
                              Point3dPtr    camPos,
                              VecPtr        camUp,
                              Point3dPtr    target );
/*/

/* ---------------------------------------------------------------------------
    Project matrix stuff
--------------------------------------------------------------------------- */

/*/
void    MTXFrustum          ( Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f );
void    MTXPerspective      ( Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f );
void    MTXOrtho            ( Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f );
/*/

/* ---------------------------------------------------------------------------
    Texture projection
--------------------------------------------------------------------------- */

/*/
void    MTXLightFrustum     ( Mtx m, f32 t, f32 b, f32 l, f32 r, f32 n,
                              f32 scaleS, f32 scaleT, f32 transS,
                              f32 transT );

void    MTXLightPerspective ( Mtx m, f32 fovY, f32 aspect, f32 scaleS,
                              f32 scaleT, f32 transS, f32 transT );

void    MTXLightOrtho       ( Mtx m, f32 t, f32 b, f32 l, f32 r, f32 scaleS,
                              f32 scaleT, f32 transS, f32 transT );
/*/

/* ---------------------------------------------------------------------------
    Vector operations
--------------------------------------------------------------------------- */

/*/
void    VECAdd              ( VecPtr a, VecPtr b, VecPtr ab );
void    VECSubtract         ( VecPtr a, VecPtr b, VecPtr a_b );
void    VECScale            ( VecPtr src, VecPtr dst, f32 scale );
void    VECNormalize        ( VecPtr src, VecPtr unit );
f32     VECSquareMag        ( VecPtr v );
f32     VECMag              ( VecPtr v );
f32     VECDotProduct       ( VecPtr a, VecPtr b );
void    VECCrossProduct     ( VecPtr a, VecPtr b, VecPtr axb );
f32     VECSquareDistance   ( VecPtr a, VecPtr b );
f32     VECDistance         ( VecPtr a, VecPtr b );
void    VECReflect          ( VecPtr src, VecPtr normal, VecPtr dst );
void    VECHalfAngle        ( VecPtr a, VecPtr b, VecPtr half );
/*/

/* ---------------------------------------------------------------------------
    Quaternions
--------------------------------------------------------------------------- */

/*/
void    QUATAdd             ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r );
void    QUATSubtract        ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r );
void    QUATMultiply        ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr pq );
void    QUATDivide          ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r );
void    QUATScale           ( QuaternionPtr q, QuaternionPtr r, f32 scale );
f32     QUATDotProduct      ( QuaternionPtr p, QuaternionPtr q );
void    QUATNormalize       ( QuaternionPtr src, QuaternionPtr unit );
void    QUATInverse         ( QuaternionPtr src, QuaternionPtr inv );
void    QUATExp             ( QuaternionPtr q, QuaternionPtr r );
void    QUATLogN            ( QuaternionPtr q, QuaternionPtr r );
void    QUATMakeClosest     ( QuaternionPtr q, QuaternionPtr qto, QuaternionPtr r );
void    QUATRotAxisRad      ( QuaternionPtr r, VecPtr axis, f32 rad );
void    QUATMtx             ( QuaternionPtr r, Mtx m );
void    QUATLerp            ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r, f32 t );
void    QUATSlerp           ( QuaternionPtr p, QuaternionPtr q, QuaternionPtr r, f32 t );
void    QUATSquad           ( QuaternionPtr p, QuaternionPtr a, QuaternionPtr b,
                              QuaternionPtr q, QuaternionPtr r, f32 t );
void    QUATCompA           ( QuaternionPtr qprev, QuaternionPtr q,
                              QuaternionPtr qnext, QuaternionPtr a );
/*/


// Dolphin OS structures and definitions

// Note: all structures are big-endian (like on GC), use swap on access!

/* ---------------------------------------------------------------------------
    OS low memory vars
--------------------------------------------------------------------------- */

#define OS_PHYSICAL_CONTEXT     0x800000C0      // OSContext *
#define OS_CURRENT_CONTEXT      0x800000D4      // OSContext *
#define OS_DEFAULT_THREAD       0x800000D8      // OSThread *
#define OS_LINK_ACTIVE          0x800000DC      // OSThreadLink
#define OS_CURRENT_THREAD       0x800000E4      // OSThread *

/* ---------------------------------------------------------------------------
    Context API
--------------------------------------------------------------------------- */

// floating point context modes
#define     OS_CONTEXT_MODE_FPU         1   // normal mode
#define     OS_CONTEXT_MODE_PSFP        2   // gekko paired-single

// context status
#define     OS_CONTEXT_STATE_FPSAVED    1   // set when FPU is saved
#define     OS_CONTEXT_STATE_EXC        2   // set when saved by exception

#define OS_CONTEXT_FRAME_SIZE     768

#pragma pack(push, 1)

// CPU context
struct OSContext
{
    // GPRs
    uint32_t     gpr[32];

    uint32_t     cr, lr, ctr, xer;

    // FPRs (or paired-single 0-part)
    union
    {
        double      fpr[32];
        uint64_t    fprAsUint[32];
    };

    uint32_t     fpscr_pad;
    uint32_t     fpscr;          // dummy in emulator

    // exception handling regs
    uint32_t     srr[2];

    // context flags
    uint16_t     mode;           // one of OS_CONTEXT_MODE*
    uint16_t     state;          // or'ed OS_CONTEXT_STATE*

    // gekko-specific regs
    uint32_t     gqr[8];         // quantization mode regs

    uint32_t    padding;

    union
    {
        double      psr[32];        // paired-single 1-part
        uint64_t    psrAsUint[32];
    };

};

struct OSThreadLink
{
    uint32_t    next;
    uint32_t    prev;
};

struct OSThreadQueue
{
    uint32_t    head;
    uint32_t    tail;
};

typedef OSThreadQueue OSMutexQueue;

struct OSThread
{
    OSContext   context;

    uint16_t    state;
    uint16_t    attr;
    int32_t     suspend;
    uint32_t    priority;
    uint32_t    base;
    uint32_t    val;

    uint32_t    queue;
    OSThreadLink link;
    OSThreadQueue queueJoin;
    uint32_t    mutex;
    OSMutexQueue queueMutex;
    OSThreadLink linkActive;

    uint32_t    stackBase;
    uint32_t    stackEnd;
};

#pragma pack(pop)

// os calls
void    OSSetCurrentContext(void);
void    OSGetCurrentContext(void);
void    OSSaveContext(void);
void    OSLoadContext(void);
void    OSClearContext(void);
void    OSInitContext(void);
void    OSLoadFPUContext(void);
void    OSSaveFPUContext(void);
void    OSFillFPUContext(void);

void    __OSContextInit(void);

/* ---------------------------------------------------------------------------
    Interrupt handling
--------------------------------------------------------------------------- */

void    OSDisableInterrupts(void);
void    OSEnableInterrupts(void);
void    OSRestoreInterrupts(void);
