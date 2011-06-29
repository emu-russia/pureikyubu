#pragma pack(1)

// all structures are little-endian Intel format

// data, which need to be compared
typedef struct CompareData
{
    u32         pc;                 // program counter
    u64         tbr;                // time
    u32         gpr[32];            // general regs
    u32         cr;                 // condition
    u32         msr;                // machine state
    u32         fpscr;              // floating point state
    u64         fp_ps0[32], ps1[32];// floating point / paired single regs
    u32         spr[1024];          // system regs
    u32         sr[16];             // segment regs
} CompareData;

// compare engine externals
typedef struct CompareControl
{
    BOOL        started;
    BOOL        server;
    HANDLE      pipe;
} CompareControl;

extern  CompareControl comp;

// compare engine API
void    COMPCreateServer();         // create compare server
void    COMPConnectClient();        // connect as client
void    COMPDisconnect();           // close connection
void    COMPDoCompare();            // sync compare

#pragma pack ()
