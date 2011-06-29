// control struct, to pass parameters
typedef struct DISA
{
    // input
    u32     op;             // PPC opcode
    u32     pc;             // current PC

    // output
    char    opcode[32];     // mnemonics
    char    operands[32];   // operands
    int     type;           // see below
    u32     disp;           // offset for branch
} DISA;

// opcode type
#define DISA_OTHER      0
#define DISA_BRANCH     1
#define DISA_LDST       2

void    dasm(DISA *d);
