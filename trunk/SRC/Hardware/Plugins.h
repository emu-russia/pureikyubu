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
extern GXSAVELOAD           GXSaveLoad;

// AX plugin
extern AXOPEN               AXOpen;
extern AXCLOSE              AXClose;
extern AXPLAYAUDIO          AXPlayAudio;
extern AXSETRATE            AXSetRate;
extern AXPLAYSTREAM         AXPlayStream;
extern AXSETVOLUME          AXSetVolume;
extern AXCONFIGURE          AXConfigure;
extern AXABOUT              AXAbout;
extern AXSAVELOAD           AXSaveLoad;

// PAD plugin
extern PADOPEN              PADOpen;
extern PADCLOSE             PADClose;
extern PADREADBUTTONS       PADReadButtons;
extern PADSETRUMBLE         PADSetRumble;
extern PADCONFIGURE         PADConfigure;
extern PADABOUT             PADAbout;
extern PADSAVELOAD          PADSaveLoad;

// DVD plugin
extern DVDOPEN              DVDOpen;
extern DVDCLOSE             DVDClose;
extern DVDSETCURRENT        DVDSetCurrent;
extern DVDISCOMPRESSED      DVDIsCompressed;
extern DVDSEEK              DVDSeek;
extern DVDREAD              DVDRead;
extern DVDOPENFILE          DVDOpenFile;
extern DVDCONFIGURE         DVDConfigure;
extern DVDABOUT             DVDAbout;
extern DVDSAVELOAD          DVDSaveLoad;

// NET plugin
// ...

void   PSInit();
void   PSShutdown();
void   PSOpen();
void   PSClose();
