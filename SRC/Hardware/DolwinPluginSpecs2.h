//  Plugins Specifications for GAMECUBE emulators.
//  Version 1.0 (for Dolwin internal use).
//
//  VERSION HISTORY :
//  -----------------
//
//  0    28/10/04 org
//  Specs for GX, AX, PAD and DVD are created, but NET also planned.
//

// do not modify this file without major reason !
// you should report Dolwin team, about all changes :
// org <kvzorganic@mail.ru>, hotquik <hotquik@hotmail.com>

#ifndef __PLUGIN_SPECS2_H__
#define __PLUGIN_SPECS2_H__

// return values are always 1 for good, and 0 for bad.

// plugin specification version
#define DOL_PLUG_VER            "1.0"

// plugin types (can be multi-purposed, types may be ORed)
#define DOL_PLUG_GX             (0x01)      // 3D Graphics
#define DOL_PLUG_AX             (0x02)      // Audio
#define DOL_PLUG_PAD            (0x04)      // Controller
#define DOL_PLUG_DVD            (0x08)      // DVD Image
#define DOL_PLUG_NET            (0x10)      // NET Interface

// plugin type detection
#define IS_DOL_PLUG_GX(type)    (type & DOL_PLUG_GX)
#define IS_DOL_PLUG_AX(type)    (type & DOL_PLUG_AX)
#define IS_DOL_PLUG_PAD(type)   (type & DOL_PLUG_PAD)
#define IS_DOL_PLUG_DVD(type)   (type & DOL_PLUG_DVD)
#define IS_DOL_PLUG_NET(type)   (type & DOL_PLUG_NET)

// plugin data. should be passed to RegisterPlugin as parameter
typedef struct PluginData
{
    // these values are passed as parameters during plugin init
    void*           display;    // handler of drawing device (HWND* in Windows, for example)
    unsigned char*  ram;        // your 24 MB memory buffer

    // these values are returned by plugin
    unsigned long   type;       // see DOL_PLUG_* types
    char*           version;    // plugin description and version string
} PluginData;

// used to verify plugin
typedef void (*REGISTERPLUGIN)(PluginData * plugData);

// ---------------------------------------------------------------------------

// GX plugin (3D graphics)

// GXOpen() should be called before emulation started, to initialize
// plugin. GXClose() is called, when emulation is stopped, to shutdown plugin.
typedef long (*GXOPEN)();
typedef void (*GXCLOSE)();

// add new data to graphics fifo. draw next primitive, if there are enough data.
// should be called, when CPU is writing any data into 0x0C008000, when CP is
// working in "linked" mode ("single-buffer" fifo). emulator should take care
// about "multi-buffer" fifo mode itself.
typedef void (*GXWRITEFIFO)(unsigned char *dataPtr, unsigned long length);

// set pointers to GX tokens.
// when fifo reaches "draw done", it will set peDrawDone to 1.
// when fifo reaches token, it will set peToken to 1, and tokenVal to token value.
// screen is updated automatically, after DrawDone or Token event.
// GX doesnt responsible for clearing token values (should be performed by emu).
typedef void (*GXSETTOKENS)(long *peDrawDone, long *peToken, unsigned short *tokenVal);

// config / about
typedef void (*GXCONFIGURE)();
typedef void (*GXABOUT)();

// ---------------------------------------------------------------------------

// AX plugin (audio)

// AXOpen() should be called before emulation started, to initialize
// plugin. AXClose() is called, when emulation is stopped, to shutdown plugin.
typedef long (*AXOPEN)();
typedef void (*AXCLOSE)();

// play DMA audio buffer (big-endian, 16-bit), AXPlayAudio(0, 0) - to stop.
typedef void (*AXPLAYAUDIO)(void * buffer, long length);

// set DMA sample rate (32000/48000), stream sample rate is always 48000.
typedef void (*AXSETRATE)(long rate);

// play stream data (raw data, read from DVD), AXPlayStream(0, 0) - to stop.
typedef void (*AXPLAYSTREAM)(void * buffer, long length);

// set stream volume (0..255), you cant set DMA volume in hardware.
typedef void (*AXSETVOLUME)(unsigned char left, unsigned char right);

// user stuff
typedef void (*AXCONFIGURE)();
typedef void (*AXABOUT)();

// ---------------------------------------------------------------------------

// PAD plugin (input)
// (padnum = 0...3)

typedef struct PADState
{
    unsigned short  button;         // combination of PAD_BUTTON*
    signed char     stickX;         // -127...127
    signed char     stickY;
    signed char     substickX;      // -127...127
    signed char     substickY;
    unsigned char   triggerLeft;    // 0...255
    unsigned char   triggerRight;
} PADState;

// controller buttons
#define PAD_BUTTON_LEFT         (0x0001)
#define PAD_BUTTON_RIGHT        (0x0002)
#define PAD_BUTTON_DOWN         (0x0004)
#define PAD_BUTTON_UP           (0x0008)
#define PAD_TRIGGER_Z           (0x0010)
#define PAD_TRIGGER_R           (0x0020)
#define PAD_TRIGGER_L           (0x0040)
#define PAD_BUTTON_A            (0x0100)
#define PAD_BUTTON_B            (0x0200)
#define PAD_BUTTON_X            (0x0400)
#define PAD_BUTTON_Y            (0x0800)
#define PAD_BUTTON_START        (0x1000)

// controller motor commands
#define PAD_MOTOR_STOP          0
#define PAD_MOTOR_RUMBLE        1
#define PAD_MOTOR_STOP_HARD     2

// PADOpen() should be called before emulation started, to initialize
// plugin. PADClose() is called, when emulation is stopped, to shutdown plugin.
typedef long (*PADOPEN)();
typedef void (*PADCLOSE)();

// read controller buttons state. returns 1, if ok, and 0, if PAD not connected
typedef long (*PADREADBUTTONS)(long padnum, PADState *state);

// controller motor. 0 returned, if rumble is not supported by PAD.
// see one of PAD_MOTOR* for allowed commands.
typedef long (*PADSETRUMBLE)(long padnum, long cmd);

// config / about
typedef void (*PADCONFIGURE)(long padnum);
typedef void (*PADABOUT)();

// ---------------------------------------------------------------------------

// DVD plugin

// DVDOpen() should be called before emulation started, to initialize
// plugin. DVDClose() is called, when emulation is stopped, to shutdown plugin.
typedef long (*DVDOPEN)();
typedef void (*DVDCLOSE)();

// set current DVD image for read/seek/open file operations
// return 1 if no errors, and 0 if cannot use file
typedef long (*DVDSETCURRENT)(char *file);

// return 1, if DVD image is compressed
typedef long (*DVDISCOMPRESSED)(char *file);

// seek and read operations on current DVD
typedef void (*DVDSEEK)(long position);
typedef void (*DVDREAD)(void *buffer, long length);

// open file in DVD root. return file position, or 0 if no such file.
// note : current DVD must be selected first!
// example use : s32 banner = DVDOpenFile("/opening.bnr");
typedef long (*DVDOPENFILE)(char *dvdfile);

// config / about
typedef void (*DVDCONFIGURE)();
typedef void (*DVDABOUT)();

#endif  // __PLUGIN_SPECS2_H__

// EOF
