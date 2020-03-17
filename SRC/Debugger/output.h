// color codes for console
enum class ConColor
{
    DBLUE = 1,
    GREEN,
    CYAN,
    RED,
    PUR,
    BROWN,
    NORM,
    GRAY,
    BLUE,
    BGREEN,
    BCYAN,
    BRED,
    BPUR,
    YEL,
    YELLOW = YEL,
    WHITE,
};

// default log filename
#define CON_LOG_FILE        "DebugSession.htm"

// console window dimensions
#define CON_WIDTH   80
#define CON_HEIGHT  60

// update regions
#define CON_UPDATE_REGS     (0x0001)    // registers window
#define CON_UPDATE_DISA     (0x0002)    // disassembly window
#define CON_UPDATE_DATA     (0x0004)    // memory window
#define CON_UPDATE_MSGS     (0x0008)    // message history
#define CON_UPDATE_EDIT     (0x0010)    // command line
#define CON_UPDATE_STAT     (0x0020)    // status line
#define CON_UPDATE_ALL      (0x003f)    // all

#define CON_LINES           1000        // size of message buffers
#define CON_LINELEN         (CON_WIDTH+1)
#define CON_TOKENCNT        5           // max amount of cmd params

// current console window identifier
typedef enum FOCUSWND { WREGS = 0, WDATA, WDISA, WCONSOLE } FOCUSWND;

// current registers window mode
typedef enum REGWNDMODE { REGMOD_GPR = 0, REGMOD_FPR, REGMOD_PSR, REGMOD_MMU, REGMOD_MAX } REGWNDMODE;

// console windows
typedef struct WINDControl
{
    bool                full;                   // "fullscreen" mode
    FOCUSWND            focus;                  // wregs, wdata, wdisa, wconsole
    REGWNDMODE          regmode;                // register window mode
    int                 regs_y;                 // Y registers window
    int                 regs_h;                 // Height registsrs window
    int                 data_y;                 // Y Data window
    int                 data_h;                 // Height Data window
    int                 disa_y;                 // Y Disassembler
    int                 disa_h;                 // Height of Disassembler
    int                 disa_sub_h;             // height subtract modifier
    int                 roll_y;                 // Y Scroller (updates automatically by con_recalcwnds() )
    int                 roll_h;                 // Height scroller, updates automaically by con_recalcwnds()
    int                 edit_y;                 // Y Edit Line
    int                 edit_h;                 // Editor Line Height
    int                 stat_y;                 // Y Status Line
    int                 stat_h;                 // Status Line Height
    uint32_t            visible;                // see CON_UPDATE_*
    uint32_t            disa_nav_hist[256];     // disassembler window navigation history
    int                 disa_nav_last;          // last address
    bool                ldst;                   // used for load/store helper
    uint32_t            ldst_disp;
} WINDControl;

typedef struct ROLLControl
{
    char    data[CON_LINES][CON_LINELEN];       // scrolling lines    
    int     rollpos;                            // Where to read (len = wind.roll_h-1)
    int     viewpos;                            // Current position of "window" which transfers to CON.buf
    char    statusline[CON_LINELEN];            // console status line
    char    editline[CON_LINELEN];              // edit line
    int     editpos;                            // position to next char in edit line
    int     editlen;                            // edit line len
    char    history[256][CON_LINELEN];          // command history
    int     historypos;                         // command history position
    int     historycur;                         // current command history position
    char    tokens[CON_TOKENCNT][CON_LINELEN];  // tokens parsed from editline[]
    int     tokencount;                         // parsed tokens count
    bool    autoscroll;                         // if TRUE, then viewpos = rollpos-1
} ROLLControl;

extern  WINDControl wind;
extern  ROLLControl roll;

// ---------------------------------------------------------------------------

// helpers
#define con_attr(fg, bg)    con.attr = (bg << 4) | fg
#define con_attr_fg(fg)     con.attr = (con.attr & ~0xf) | fg
#define con_attr_bg(bg)     con.attr = (con.attr & ~(0xf << 4)) | (bg << 4)

int     con_wraproll(int roll, int value);
void    con_set_disa_cur(uint32_t addr);
void    con_recalc_wnds();
void    con_blt_region(int regY, int regH);
void    con_nextline();
void    con_printchar(char ch);
void    con_printline(const char *text);
void    con_gotoxy(int X, int Y);
void    con_print_at(int X, int Y, const char *text);
void    con_status(const char *txt);
void    con_cursorxy(int x, int y);
void    con_fill_line(int y);
void    con_clear_line(int y, uint16_t attr=7);
void    con_printf_at(int x, int y, const char *txt, ...);
void    con_set_autoscroll(bool value);
void    con_add_roller_line(const char *txt, int err);
void    con_change_focus(FOCUSWND newfocus);
void    con_fullscreen(bool full);
void    con_update(uint32_t mask);
void    con_refresh(bool showpc=false);
void    con_error(const char *txt, ...);
void    con_print(ConColor col, const char *txt, ...);
