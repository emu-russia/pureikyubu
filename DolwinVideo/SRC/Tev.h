enum
{
    TEV_DECAL = 0,
    TEV_MODULATE,
    TEV_BLEND,          // enable alpha pass
    TEV_REPLACE,
    TEV_PASSCLR
};

typedef struct
{
    int     mode;       // one of listed above
    int     tex;        // 0..7 texture coord
    int     ras;        // 0 = COL0A0, 1 = COL1A1
    Color   col[3];     // 3 constant colors
} TEVStage;

extern  TEVStage tevs[16];

void    TEVApprox();
void    TEVSelectOutput(int stage);
