// hardware registers base (physical address)
#define HW_BASE         0x0C000000

// max known GC HW address is 0x0C008004 (fifo), so 0x8010 will be enough.
// note : it must not be greater 0xffff, unless you need to change code.
#define HW_MAX_KNOWN    0x8010

// register traps
extern void (__fastcall *hw_read8  [HW_MAX_KNOWN])(uint32_t, uint32_t *);
extern void (__fastcall *hw_write8 [HW_MAX_KNOWN])(uint32_t, uint32_t);
extern void (__fastcall *hw_read16 [HW_MAX_KNOWN])(uint32_t, uint32_t *);
extern void (__fastcall *hw_write16[HW_MAX_KNOWN])(uint32_t, uint32_t);
extern void (__fastcall *hw_read32 [HW_MAX_KNOWN])(uint32_t, uint32_t *);
extern void (__fastcall *hw_write32[HW_MAX_KNOWN])(uint32_t, uint32_t);

// hardware API
void    HWSetTrap(
            uint32_t type,                                       // 8, 16 or 32
            uint32_t addr,                                       // physical address of trap
            void (__fastcall *rdTrap)(uint32_t, uint32_t *) = NULL,   // register read trap
            void (__fastcall *wrTrap)(uint32_t, uint32_t)   = NULL);  // register write trap
void    HWOpen();
void    HWClose();
void    HWUpdate();
void    HWEnableUpdate(BOOL en);
