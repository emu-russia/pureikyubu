#include "DolwinPluginSpecs2.h"

extern PluginData           plug;

extern REGISTERPLUGIN       RegisterPlugin;

// GX plugin
extern GXOPEN               GXOpen;
extern GXCLOSE              GXClose;
extern GXWRITEFIFO          GXWriteFifo;
extern GXSETTOKENS          GXSetTokens;
extern GXCONFIGURE          GXConfigure;
extern GXABOUT              GXAbout;

// AX plugin
extern AXOPEN               AXOpen;
extern AXCLOSE              AXClose;
extern AXPLAYAUDIO          AXPlayAudio;
extern AXSETRATE            AXSetRate;
extern AXPLAYSTREAM         AXPlayStream;
extern AXSETVOLUME          AXSetVolume;
extern AXCONFIGURE          AXConfigure;
extern AXABOUT              AXAbout;

// PAD plugin
extern PADOPEN              PADOpen;
extern PADCLOSE             PADClose;
extern PADREADBUTTONS       PADReadButtons;
extern PADSETRUMBLE         PADSetRumble;
extern PADCONFIGURE         PADConfigure;
extern PADABOUT             PADAbout;

// DVD plugin
extern DVDOPEN              DVDOpen;
extern DVDCLOSE             DVDClose;
extern DVDSETCURRENT        DVDSetCurrent;
extern DVDSEEK              DVDSeek;
extern DVDREAD              DVDRead;
extern DVDOPENFILE          DVDOpenFile;
extern DVDCONFIGURE         DVDConfigure;
extern DVDABOUT             DVDAbout;

void   PSInit();
void   PSShutdown();
void   PSOpen();
void   PSClose();
