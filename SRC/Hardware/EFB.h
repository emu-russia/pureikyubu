#define EFB_BASE    0x08000000
#define EFB_SIZE    0x200000        // 2 mb
#define EFB_MASK    0x1fffff

// peeks and pokes
void    EFBPeek8(u32 ofs, u32 *reg);
void    EFBPeek16(u32 ofs, u32 *reg);
void    EFBPeek32(u32 ofs, u32 *reg);
void    EFBPoke8(u32 ofs, u32 data);
void    EFBPoke16(u32 ofs, u32 data);
void    EFBPoke32(u32 ofs, u32 data);
