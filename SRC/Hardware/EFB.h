#define EFB_BASE    0x08000000
#define EFB_SIZE    0x200000        // 2 mb
#define EFB_MASK    0x1fffff

// peeks and pokes
void    EFBPeek8(uint32_t ofs, uint32_t *reg);
void    EFBPeek16(uint32_t ofs, uint32_t *reg);
void    EFBPeek32(uint32_t ofs, uint32_t *reg);
void    EFBPoke8(uint32_t ofs, uint32_t data);
void    EFBPoke16(uint32_t ofs, uint32_t data);
void    EFBPoke32(uint32_t ofs, uint32_t data);
