void    os_ignore();
void    os_ret0();
void    os_ret1();
void    os_trap();

// used for HLE top 10 calculation (hitrate index)
enum HLEHitrate
{
    HLE_OS_DISABLE_INTERRUPTS = 1,
    HLE_OS_ENABLE_INTERRUPTS,
    HLE_OS_RESTORE_INTERRUPTS,

    HLE_OS_SET_CURRENT_CONTEXT,
    HLE_OS_GET_CURRENT_CONTEXT,
    HLE_OS_SAVE_CONTEXT,
    HLE_OS_LOAD_CONTEXT,
    HLE_OS_CLEAR_CONTEXT,
    HLE_OS_INIT_CONTEXT,

    HLE_MEMCPY,
    HLE_MEMSET,
    HLE_SIN,
    HLE_COS,
    HLE_MODF,
    HLE_FREXP,
    HLE_LDEXP,
    HLE_FLOOR,
    HLE_CEIL,

    HLE_MTX_IDENTITY,
    HLE_MTX_COPY,
    HLE_MTX_CONCAT,
    HLE_MTX_TRANSPOSE,
    HLE_MTX_INVERSE,
    HLE_MTX_INVXPOSE,

    HLE_HITRATE_MAX
};

#define HLEHit(index)   hle.hitrate[index]++
void    HLEResetHitrate();
void    HLEGetTop10(int toplist[10]);
const char* HLEGetHitNameByIndex(int idx);

// HLE state variables
typedef struct HLEControl
{
    bool        lastHwAssert;   // saved HW_ASSERT uvar flag

    // current loaded map file
    char        mapfile[0x1000];
    
    // number of HLE hits in last frame
    int         hitrate[HLE_HITRATE_MAX];
    int         top10[10];
} HLEControl;

extern  HLEControl hle;

void    HLESetCall(const char *name, void (*call)());
void    HLEOpen();
void    HLEClose();
void    HLEExecuteCallback(uint32_t entryPoint);
