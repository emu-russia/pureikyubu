// texture entry
typedef struct
{
    u32     ramAddr;
    u8      *rawData;
    Color   *rgbaData;      // allocated
    int     fmt, tfmt;
    int     w, h, dw, dh;
    float   ds, dt;
    UINT    bind;
} TexEntry;

// texture formats
enum
{
    TF_I4 = 0,
    TF_I8,
    TF_IA4,
    TF_IA8,
    TF_RGB565,
    TF_RGB5A3,
    TF_RGBA8,
    TF_C4 = 8,
    TF_C8,
    TF_C14,
    TF_CMPR = 14    // s3tc
};

typedef struct
{
    unsigned    t : 2;
} S3TC_TEX;

typedef struct
{
    u16     rgb0;       // color 2
    u16     rgb1;       // color 1
    u8      row[4];
} S3TC_BLK;


// ---------------------------------------------------------------------------

// interface to application

extern  TexEntry *tID[8];

void    TexInit();
void    TexFree();
void    RebindTexture(unsigned id);
void    LoadTexture(u32 addr, int id, int fmt, int width, int height);
void    LoadTlut(u32 addr, u32 tmem, u32 cnt);
