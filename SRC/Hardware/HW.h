// hardware registers base (physical address)
#define HW_BASE         0x0C000000

extern	DSP::DspCore* dspCore;      // instance of dsp core

// hardware API
void    HWOpen(HWConfig * config);
void    HWClose();
void    HWUpdate();
void    HWEnableUpdate(bool en);
