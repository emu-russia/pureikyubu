// hardware registers base (physical address)
#define HW_BASE         0x0C000000

// max known GC HW address is 0x0C008004 (fifo), so 0x8010 will be enough.
// note : it must not be greater 0xffff, unless you need to change code.
#define HW_MAX_KNOWN    0x8010

// register traps
extern void (__fastcall *hw_read8  [HW_MAX_KNOWN])(u32, u32 *);
extern void (__fastcall *hw_write8 [HW_MAX_KNOWN])(u32, u32);
extern void (__fastcall *hw_read16 [HW_MAX_KNOWN])(u32, u32 *);
extern void (__fastcall *hw_write16[HW_MAX_KNOWN])(u32, u32);
extern void (__fastcall *hw_read32 [HW_MAX_KNOWN])(u32, u32 *);
extern void (__fastcall *hw_write32[HW_MAX_KNOWN])(u32, u32);

// hardware API
void    HWSetTrap(
            u32 type,                                       // 8, 16 or 32
            u32 addr,                                       // physical address of trap
            void (__fastcall *rdTrap)(u32, u32 *) = NULL,   // register read trap
            void (__fastcall *wrTrap)(u32, u32)   = NULL);  // register write trap
void    HWOpen();
void    HWClose();
void    HWUpdate();
void    HWEnableUpdate(BOOL en);
