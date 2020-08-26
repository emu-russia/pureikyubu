// all gfx registers definitions here

#pragma once

typedef void (*GXDrawDoneCallback)();
typedef void (*GXDrawTokenCallback)(uint16_t tokenValue);

extern GXDrawDoneCallback GxDrawDone;
extern GXDrawTokenCallback GxDrawToken;

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// register names

// ---------------------------------------------------------------------------
// CP Memory Space
// ---------------------------------------------------------------------------

#define CP_MATIDX_A                     0x30    // Pos / Texture Matrix Index 0-3
#define CP_MATIDX_B                     0x40    // Texture Matrix Index 4-7
#define CP_VCD_LO                       0x50    // Vertex Descriptor (VCD) low 
#define CP_VCD_HI                       0x60    // Vertex Descriptor (VCD) high
#define CP_VAT0_A                       0x70    // Vertex Attribute Table (VAT) group A
#define CP_VAT1_A                       0x71
#define CP_VAT2_A                       0x72
#define CP_VAT3_A                       0x73
#define CP_VAT4_A                       0x74
#define CP_VAT5_A                       0x75
#define CP_VAT6_A                       0x76
#define CP_VAT7_A                       0x77
#define CP_VAT0_B                       0x80    // Vertex Attribute Table (VAT) group B
#define CP_VAT1_B                       0x81
#define CP_VAT2_B                       0x82
#define CP_VAT3_B                       0x83
#define CP_VAT4_B                       0x84
#define CP_VAT5_B                       0x85
#define CP_VAT6_B                       0x86
#define CP_VAT7_B                       0x87
#define CP_VAT0_C                       0x90    // Vertex Attribute Table (VAT) group C
#define CP_VAT1_C                       0x91
#define CP_VAT2_C                       0x92
#define CP_VAT3_C                       0x93
#define CP_VAT4_C                       0x94
#define CP_VAT5_C                       0x95
#define CP_VAT6_C                       0x96
#define CP_VAT7_C                       0x97
#define CP_ARRAY_BASE                   0xA0
#define CP_ARRAY_STRIDE                 0xB0
#define CP_NUMCOL                       0xB2    // number of colors attributes
#define CP_NUMTEX                       0xB4    // number of texcoord attributes

// vertex attribute types (from VCD register)
enum
{
    VCD_NONE = 0,           // attribute stage disabled
    VCD_DIRECT,             // direct data
    VCD_INDX8,              // 8-bit indexed data
    VCD_INDX16              // 16-bit indexed data (rare)
};

// vertex attribute "cnt" (from VAT register)
enum
{
    VCNT_POS_XY     = 0,
    VCNT_POS_XYZ    = 1,
    VCNT_NRM_XYZ    = 0,
    VCNT_NRM_NBT    = 1,    // index is NBT
    VCNT_NRM_NBT3   = 2,    // index is one from N/B/T
    VCNT_CLR_RGB    = 0,
    VCNT_CLR_RGBA   = 1,
    VCNT_TEX_S      = 0,
    VCNT_TEX_ST     = 1
};

// vertex attribute "fmt" (from VAT register)
enum
{
    VFMT_U8     = 0,
    VFMT_S8     = 1,
    VFMT_U16    = 2,
    VFMT_S16    = 3,
    VFMT_F32    = 4,

    VFMT_RGB565 = 0,
    VFMT_RGB8   = 1,
    VFMT_RGBX8  = 2,
    VFMT_RGBA4  = 3,
    VFMT_RGBA6  = 4,
    VFMT_RGBA8  = 5
};

// ---------------------------------------------------------------------------
// BP Memory Space
// ---------------------------------------------------------------------------

#define BP_GEN_MODE                     0x00

#define BP_SU_SCIS0                     0x20
#define BP_SU_SCIS1                     0x21

#define SU_SSIZE_0                      0x30    // s/t coord scale 
#define SU_TSIZE_0                      0x31
#define SU_SSIZE_1                      0x32
#define SU_TSIZE_1                      0x33
#define SU_SSIZE_2                      0x34
#define SU_TSIZE_2                      0x35
#define SU_SSIZE_3                      0x36
#define SU_TSIZE_3                      0x37
#define SU_SSIZE_4                      0x38
#define SU_TSIZE_4                      0x39
#define SU_SSIZE_5                      0x3A
#define SU_TSIZE_5                      0x3B
#define SU_SSIZE_6                      0x3C
#define SU_TSIZE_6                      0x3D
#define SU_SSIZE_7                      0x3E
#define SU_TSIZE_7                      0x3F

#define PE_ZMODE_ID                     0x40
#define PE_CMODE0                       0x41
#define PE_CMODE1                       0x42

#define PE_DONE                         0x45
#define PE_TOKEN                        0x47
#define PE_TOKEN_INT                    0x48
#define PE_COPY_CLEAR_AR                0x4F
#define PE_COPY_CLEAR_GB                0x50
#define PE_COPY_CLEAR_Z                 0x51

#define TX_LOADTLUT0                    0x64    // tlut base in memory
#define TX_LOADTLUT1                    0x65    // tmem ofs and size

#define TX_SETMODE_0_0                  0x80    // wrap (mode)
#define TX_SETMODE_0_1                  0x81
#define TX_SETMODE_0_2                  0x82
#define TX_SETMODE_0_3                  0x83
#define TX_SETMODE_0_4                  0xA0
#define TX_SETMODE_0_5                  0xA1
#define TX_SETMODE_0_6                  0xA2
#define TX_SETMODE_0_7                  0xA3

#define TX_SETIMAGE_0_0                 0x88    // texture width, height, format
#define TX_SETIMAGE_0_1                 0x89
#define TX_SETIMAGE_0_2                 0x8A
#define TX_SETIMAGE_0_3                 0x8B
#define TX_SETIMAGE_0_4                 0xA8
#define TX_SETIMAGE_0_5                 0xA9
#define TX_SETIMAGE_0_6                 0xAA
#define TX_SETIMAGE_0_7                 0xAB

#define TX_SETIMAGE_3_0                 0x94    // texture_map >> 5, physical address
#define TX_SETIMAGE_3_1                 0x95
#define TX_SETIMAGE_3_2                 0x96
#define TX_SETIMAGE_3_3                 0x97
#define TX_SETIMAGE_3_4                 0xB4
#define TX_SETIMAGE_3_5                 0xB5
#define TX_SETIMAGE_3_6                 0xB6
#define TX_SETIMAGE_3_7                 0xB7

#define TX_SETTLUT_0                    0x98    // bind tlut with texture
#define TX_SETTLUT_1                    0x99
#define TX_SETTLUT_2                    0x9A
#define TX_SETTLUT_3                    0x9B
#define TX_SETTLUT_4                    0xB8
#define TX_SETTLUT_5                    0xB9
#define TX_SETTLUT_6                    0xBA
#define TX_SETTLUT_7                    0xBB

enum
{
    TLUT_16 = 1,
    TLUT_32 = 2,
    TLUT_64 = 4,
    TLUT_128 = 8,
    TLUT_256 = 16,
    TLUT_512 = 32,
    TLUT_1024 = 64,
    TLUT_2048 = 128,
    TLUT_4096 = 256,
    TLUT_8192 = 512,
    TLUT_16384 = 1024
};

// ---------------------------------------------------------------------------
// XF Memory Space
// ---------------------------------------------------------------------------

// 0x0000...0x03FF - Matrix memory.
// This block is formed by the matrix memory. 
// Its address range is 0 to 1 k, but only 256 entries are used. 
// This memory is organized in a 64 entry by four 32b words

// 0x0400...0x04FF - Normal matrix memory.
// This block of memory is the normal matrix memory. 
// It is organized as 32 rows of 3 words

// 0x0500...0x05FF - Texture post-transform matrix memory.
// This block of memory holds the dual texture transform matrices. 
// The format is identical to the first block of matrix memory. 
// There are also 64 rows of 4 words for these matrices

// Light parameter registers.

// LIGHT 0 DATA
#define XF_LIGHT0                       0x0600  // reserved
#define XF_0x0601                       0x0601  // reserved
#define XF_0x0602                       0x0602  // reserved
#define XF_LIGHT0_RGBA                  0x0603  // RGBA (8b/comp)
#define XF_LIGHT0_A0                    0x0604  // cos atten a0
#define XF_LIGHT0_A1                    0x0605  // cos atten a1
#define XF_LIGHT0_A2                    0x0606  // cos atten a2
#define XF_LIGHT0_K0                    0x0607  // dist atten k0
#define XF_LIGHT0_K1                    0x0608  // dist atten k1
#define XF_LIGHT0_K2                    0x0609  // dist atten k2
#define XF_LIGHT0_LPX                   0x060A  // x light pos, or inf 1dir x
#define XF_LIGHT0_LPY                   0x060B  // y light pos, or inf 1dir y
#define XF_LIGHT0_LPZ                   0x060C  // z light pos, or inf 1dir z
#define XF_LIGHT0_DHX                   0x060D  // light dir x, or 1/2 angle x
#define XF_LIGHT0_DHY                   0x060E  // light dir y, or 1/2 angle y
#define XF_LIGHT0_DHZ                   0x060F  // light dir z, or 1/2 angle z

// LIGHT 1 DATA
#define XF_LIGHT1                       0x0610  // reserved
#define XF_0x0611                       0x0611  // reserved
#define XF_0x0612                       0x0612  // reserved
#define XF_LIGHT1_RGBA                  0x0613  // RGBA (8b/comp)
#define XF_LIGHT1_A0                    0x0614  // cos atten a0
#define XF_LIGHT1_A1                    0x0615  // cos atten a1
#define XF_LIGHT1_A2                    0x0616  // cos atten a2
#define XF_LIGHT1_K0                    0x0617  // dist atten k0
#define XF_LIGHT1_K1                    0x0618  // dist atten k1
#define XF_LIGHT1_K2                    0x0619  // dist atten k2
#define XF_LIGHT1_LPX                   0x061A  // x light pos, or inf 1dir x
#define XF_LIGHT1_LPY                   0x061B  // y light pos, or inf 1dir y
#define XF_LIGHT1_LPZ                   0x061C  // z light pos, or inf 1dir z
#define XF_LIGHT1_DHX                   0x061D  // light dir x, or 1/2 angle x
#define XF_LIGHT1_DHY                   0x061E  // light dir y, or 1/2 angle y
#define XF_LIGHT1_DHZ                   0x061F  // light dir z, or 1/2 angle z

// LIGHT 2 DATA
#define XF_LIGHT2                       0x0620  // reserved
#define XF_0x0621                       0x0621  // reserved
#define XF_0x0622                       0x0622  // reserved
#define XF_LIGHT2_RGBA                  0x0623  // RGBA (8b/comp)
#define XF_LIGHT2_A0                    0x0624  // cos atten a0
#define XF_LIGHT2_A1                    0x0625  // cos atten a1
#define XF_LIGHT2_A2                    0x0626  // cos atten a2
#define XF_LIGHT2_K0                    0x0627  // dist atten k0
#define XF_LIGHT2_K1                    0x0628  // dist atten k1
#define XF_LIGHT2_K2                    0x0629  // dist atten k2
#define XF_LIGHT2_LPX                   0x062A  // x light pos, or inf 1dir x
#define XF_LIGHT2_LPY                   0x062B  // y light pos, or inf 1dir y
#define XF_LIGHT2_LPZ                   0x062C  // z light pos, or inf 1dir z
#define XF_LIGHT2_DHX                   0x062D  // light dir x, or 1/2 angle x
#define XF_LIGHT2_DHY                   0x062E  // light dir y, or 1/2 angle y
#define XF_LIGHT2_DHZ                   0x062F  // light dir z, or 1/2 angle z

// LIGHT 3 DATA
#define XF_LIGHT3                       0x0630  // reserved
#define XF_0x0631                       0x0631  // reserved
#define XF_0x0632                       0x0632  // reserved
#define XF_LIGHT3_RGBA                  0x0633  // RGBA (8b/comp)
#define XF_LIGHT3_A0                    0x0634  // cos atten a0
#define XF_LIGHT3_A1                    0x0635  // cos atten a1
#define XF_LIGHT3_A2                    0x0636  // cos atten a2
#define XF_LIGHT3_K0                    0x0637  // dist atten k0
#define XF_LIGHT3_K1                    0x0638  // dist atten k1
#define XF_LIGHT3_K2                    0x0639  // dist atten k2
#define XF_LIGHT3_LPX                   0x063A  // x light pos, or inf 1dir x
#define XF_LIGHT3_LPY                   0x063B  // y light pos, or inf 1dir y
#define XF_LIGHT3_LPZ                   0x063C  // z light pos, or inf 1dir z
#define XF_LIGHT3_DHX                   0x063D  // light dir x, or 1/2 angle x
#define XF_LIGHT3_DHY                   0x063E  // light dir y, or 1/2 angle y
#define XF_LIGHT3_DHZ                   0x063F  // light dir z, or 1/2 angle z

// LIGHT 4 DATA
#define XF_LIGHT4                       0x0640  // reserved
#define XF_0x0641                       0x0641  // reserved
#define XF_0x0642                       0x0642  // reserved
#define XF_LIGHT4_RGBA                  0x0643  // RGBA (8b/comp)
#define XF_LIGHT4_A0                    0x0644  // cos atten a0
#define XF_LIGHT4_A1                    0x0645  // cos atten a1
#define XF_LIGHT4_A2                    0x0646  // cos atten a2
#define XF_LIGHT4_K0                    0x0647  // dist atten k0
#define XF_LIGHT4_K1                    0x0648  // dist atten k1
#define XF_LIGHT4_K2                    0x0649  // dist atten k2
#define XF_LIGHT4_LPX                   0x064A  // x light pos, or inf 1dir x
#define XF_LIGHT4_LPY                   0x064B  // y light pos, or inf 1dir y
#define XF_LIGHT4_LPZ                   0x064C  // z light pos, or inf 1dir z
#define XF_LIGHT4_DHX                   0x064D  // light dir x, or 1/2 angle x
#define XF_LIGHT4_DHY                   0x064E  // light dir y, or 1/2 angle y
#define XF_LIGHT4_DHZ                   0x064F  // light dir z, or 1/2 angle z

// LIGHT 5 DATA
#define XF_LIGHT5                       0x0650  // reserved
#define XF_0x0651                       0x0651  // reserved
#define XF_0x0652                       0x0652  // reserved
#define XF_LIGHT5_RGBA                  0x0653  // RGBA (8b/comp)
#define XF_LIGHT5_A0                    0x0654  // cos atten a0
#define XF_LIGHT5_A1                    0x0655  // cos atten a1
#define XF_LIGHT5_A2                    0x0656  // cos atten a2
#define XF_LIGHT5_K0                    0x0657  // dist atten k0
#define XF_LIGHT5_K1                    0x0658  // dist atten k1
#define XF_LIGHT5_K2                    0x0659  // dist atten k2
#define XF_LIGHT5_LPX                   0x065A  // x light pos, or inf 1dir x
#define XF_LIGHT5_LPY                   0x065B  // y light pos, or inf 1dir y
#define XF_LIGHT5_LPZ                   0x065C  // z light pos, or inf 1dir z
#define XF_LIGHT5_DHX                   0x065D  // light dir x, or 1/2 angle x
#define XF_LIGHT5_DHY                   0x065E  // light dir y, or 1/2 angle y
#define XF_LIGHT5_DHZ                   0x065F  // light dir z, or 1/2 angle z

// LIGHT 6 DATA
#define XF_LIGHT6                       0x0660  // reserved
#define XF_0x0661                       0x0661  // reserved
#define XF_0x0662                       0x0662  // reserved
#define XF_LIGHT6_RGBA                  0x0663  // RGBA (8b/comp)
#define XF_LIGHT6_A0                    0x0664  // cos atten a0
#define XF_LIGHT6_A1                    0x0665  // cos atten a1
#define XF_LIGHT6_A2                    0x0666  // cos atten a2
#define XF_LIGHT6_K0                    0x0667  // dist atten k0
#define XF_LIGHT6_K1                    0x0668  // dist atten k1
#define XF_LIGHT6_K2                    0x0669  // dist atten k2
#define XF_LIGHT6_LPX                   0x066A  // x light pos, or inf 1dir x
#define XF_LIGHT6_LPY                   0x066B  // y light pos, or inf 1dir y
#define XF_LIGHT6_LPZ                   0x066C  // z light pos, or inf 1dir z
#define XF_LIGHT6_DHX                   0x066D  // light dir x, or 1/2 angle x
#define XF_LIGHT6_DHY                   0x066E  // light dir y, or 1/2 angle y
#define XF_LIGHT6_DHZ                   0x066F  // light dir z, or 1/2 angle z

// LIGHT 7 DATA
#define XF_LIGHT7                       0x0670  // reserved
#define XF_0x0671                       0x0671  // reserved
#define XF_0x0672                       0x0672  // reserved
#define XF_LIGHT7_RGBA                  0x0673  // RGBA (8b/comp)
#define XF_LIGHT7_A0                    0x0674  // cos atten a0
#define XF_LIGHT7_A1                    0x0675  // cos atten a1
#define XF_LIGHT7_A2                    0x0676  // cos atten a2
#define XF_LIGHT7_K0                    0x0677  // dist atten k0
#define XF_LIGHT7_K1                    0x0678  // dist atten k1
#define XF_LIGHT7_K2                    0x0679  // dist atten k2
#define XF_LIGHT7_LPX                   0x067A  // x light pos, or inf 1dir x
#define XF_LIGHT7_LPY                   0x067B  // y light pos, or inf 1dir y
#define XF_LIGHT7_LPZ                   0x067C  // z light pos, or inf 1dir z
#define XF_LIGHT7_DHX                   0x067D  // light dir x, or 1/2 angle x
#define XF_LIGHT7_DHY                   0x067E  // light dir y, or 1/2 angle y
#define XF_LIGHT7_DHZ                   0x067F  // light dir z, or 1/2 angle z

// XF 0x0680...0x07FF reserved

// Color channel registers.

#define XF_NUMCOLS                      0x1009  // Selects the number of output colors
#define XF_AMBIENT0                     0x100A  // RGBA (8b/comp) ambient color 0
#define XF_AMBIENT1                     0x100B  // RGBA (8b/comp) ambient color 1
#define XF_MATERIAL0                    0x100C  // RGBA (8b/comp) material color 0
#define XF_MATERIAL1                    0x100D  // RGBA (8b/comp) material color 1
#define XF_COLOR0CNTL                   0x100E  // COLOR0 channel control
#define XF_COLOR1CNTL                   0x100F  // COLOR1 channel control
#define XF_ALPHA0CNTL                   0x1010  // ALPHA0 channel control
#define XF_ALPHA1CNTL                   0x1011  // ALPHA1 channel control

#define XF_DUALTEX                      0x1012  // enable tex post-transform
#define XF_MATINDEX_A                   0x1018  // Position / Tex coord 0-3 mat index
#define XF_MATINDEX_B                   0x1019  // Tex coord 4-7 mat index
#define XF_VIEWPORT                     0x101A
#define XF_VIEWPORT_SCALE_X             (XF_VIEWPORT + 0)
#define XF_VIEWPORT_SCALE_Y             (XF_VIEWPORT + 1)
#define XF_VIEWPORT_SCALE_Z             (XF_VIEWPORT + 2)
#define XF_VIEWPORT_OFFSET_X            (XF_VIEWPORT + 3)
#define XF_VIEWPORT_OFFSET_Y            (XF_VIEWPORT + 4)
#define XF_VIEWPORT_OFFSET_Z            (XF_VIEWPORT + 5)
#define XF_PROJECTION                   0x1020  // Projection data (not matrix)
#define XF_NUMTEX                       0x103F  // active texgens
#define XF_TEXGEN0                      0x1040
#define XF_TEXGEN1                      0x1041
#define XF_TEXGEN2                      0x1042
#define XF_TEXGEN3                      0x1043
#define XF_TEXGEN4                      0x1044
#define XF_TEXGEN5                      0x1045
#define XF_TEXGEN6                      0x1046
#define XF_TEXGEN7                      0x1047
#define XF_DUALGEN0                     0x1050  // dual texgen setup
#define XF_DUALGEN1                     0x1051
#define XF_DUALGEN2                     0x1052
#define XF_DUALGEN3                     0x1053
#define XF_DUALGEN4                     0x1054
#define XF_DUALGEN5                     0x1055
#define XF_DUALGEN6                     0x1056
#define XF_DUALGEN7                     0x1057

// texgen inrow enum
enum
{
    XF_TEXGEN_GEOM_INROW = 0,
    XF_TEXGEN_NORMAL_INROW,
    XF_TEXGEN_COLORS_INROW,
    XF_TEXGEN_BINORMAL_T_INROW,
    XF_TEXGEN_BINORMAL_B_INROW,
    XF_TEXGEN_TEX0_INROW,
    XF_TEXGEN_TEX1_INROW,
    XF_TEXGEN_TEX2_INROW,
    XF_TEXGEN_TEX3_INROW,
    XF_TEXGEN_TEX4_INROW,
    XF_TEXGEN_TEX5_INROW,
    XF_TEXGEN_TEX6_INROW,
    XF_TEXGEN_TEX7_INROW

    // other reserved for future
};

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// helpful structures

// VCD_LO register layout
typedef union
{
    struct
    {
        unsigned    pmidx : 1;
        unsigned    t0midx : 1;
        unsigned    t1midx : 1;
        unsigned    t2midx : 1;
        unsigned    t3midx : 1;
        unsigned    t4midx : 1;
        unsigned    t5midx : 1;
        unsigned    t6midx : 1;
        unsigned    t7midx : 1;
        unsigned    pos : 2;
        unsigned    nrm : 2;
        unsigned    col0 : 2;
        unsigned    col1 : 2;
        unsigned    rsrv : 15;
    };
    uint32_t     vcdlo;
} VCD_LO;

// VCD_HI register layout
typedef union
{
    struct
    {
        unsigned    tex0 : 2;
        unsigned    tex1 : 2;
        unsigned    tex2 : 2;
        unsigned    tex3 : 2;
        unsigned    tex4 : 2;
        unsigned    tex5 : 2;
        unsigned    tex6 : 2;
        unsigned    tex7 : 2;
        unsigned    rsrv : 16;
    };
    uint32_t     vcdhi;
} VCD_HI;

// VAT_A register layout
typedef union
{
    struct
    {
        unsigned    poscnt : 1;
        unsigned    posfmt : 3;
        unsigned    posshft : 5;
        unsigned    nrmcnt : 1;
        unsigned    nrmfmt : 3;
        unsigned    col0cnt : 1;
        unsigned    col0fmt : 3;
        unsigned    col1cnt : 1;
        unsigned    col1fmt : 3;
        unsigned    tex0cnt : 1;
        unsigned    tex0fmt : 3;
        unsigned    tex0shft : 5;
        unsigned    bytedeq : 1;        // always 1
        unsigned    nrmidx3 : 1;
    };
    uint32_t     vata;
} VAT_A;

// VAT_B register layout
typedef union
{
    struct
    {
        unsigned    tex1cnt : 1;
        unsigned    tex1fmt : 3;
        unsigned    tex1shft : 5;
        unsigned    tex2cnt : 1;
        unsigned    tex2fmt : 3;
        unsigned    tex2shft : 5;
        unsigned    tex3cnt : 1;
        unsigned    tex3fmt : 3;
        unsigned    tex3shft : 5;
        unsigned    tex4cnt : 1;
        unsigned    tex4fmt : 3;
        unsigned    vcache : 1;         // always 1
    };
    uint32_t     vatb;
} VAT_B;

// VAT_C register layout
typedef union
{
    struct
    {
        unsigned    tex4shft : 5;
        unsigned    tex5cnt : 1;
        unsigned    tex5fmt : 3;
        unsigned    tex5shft : 5;
        unsigned    tex6cnt : 1;
        unsigned    tex6fmt : 3;
        unsigned    tex6shft : 5;
        unsigned    tex7cnt : 1;
        unsigned    tex7fmt : 3;
        unsigned    tex7shft : 5;
    };
    uint32_t     vatc;
} VAT_C;

// texture params
typedef union
{
    struct
    {
        unsigned    width : 10;
        unsigned    height : 10;
        unsigned    fmt : 4;
        unsigned    op : 8;
    };
    uint32_t     hex;
} TEXIMAGE0;

// texture location
typedef union
{
    struct
    {
        unsigned    base : 24;
        unsigned    op : 8;
    };
    uint32_t     hex;
} TEXIMAGE3;

// gen mode
typedef union
{
    struct
    {
        unsigned    ntex : 4;
        unsigned    ncol : 5;
        unsigned    msen : 1;
        unsigned    ntev : 4;
        unsigned    cull : 2;
        unsigned    nbmp : 3;
        unsigned    zfreeze : 5;
        unsigned    rid : 8;        // reserved
    };
    uint32_t     hex;
} GenMode;

// texture generator setup
typedef union
{
    struct
    {
        unsigned    rsrv : 1;
        unsigned    pojection : 1;  // 0:st (2x4), 1:stq (3x4)
        unsigned    in_form : 2;    // 0:(a,b,1,1), 1:(a,b,c,1)
        unsigned    type : 3;       // 0:regular, 1:bump, 2,3: toon
        unsigned    src_row : 5;    // see INROW
        unsigned    emboss_src : 3;
        unsigned    emboss_light : 3;
        unsigned    rid : 14;
    };
    uint32_t     hex;
} TexGen;

// texture mode 0
typedef union
{
    struct
    {
        unsigned    wrap_s : 2;
        unsigned    wrap_t : 2;
        unsigned    mag : 1;
        unsigned    min : 3;
        unsigned    diaglod : 1;
        unsigned    lodbias : 10;
        unsigned    maxaniso : 2;
        unsigned    lodclamp : 3;
        unsigned    rid : 8;
    };
    uint32_t     hex;
} TexMode0;

typedef union
{
    struct
    {
        unsigned    dualidx : 6;    // index in texture memory
        unsigned    unused : 2; 
        unsigned    norm : 1;       // normalize result
        unsigned    rsrv : 23;
    };
    uint32_t     hex;
} DualTex;

// 0x64
typedef union
{
    struct
    {
        unsigned    base : 20;
    };
    uint32_t     hex;
} LoadTlut0;

// 0x65
typedef union
{
    struct
    {
        unsigned    tmem : 10;
        unsigned    count : 10;
    };
    uint32_t     hex;
} LoadTlut1;

typedef union
{
    struct
    {
        unsigned    tmem : 10;
        unsigned    fmt : 2;
    };
    uint32_t     hex;
} SetTlut;

// 0x20
typedef union
{
    struct
    {
        unsigned    suy : 12;
        unsigned    sux : 12;
        unsigned    rid : 8;
    };
    uint32_t     scis0;
} SU_SCIS0;

// 0x21
typedef union
{
    struct
    {
        unsigned    suh : 12;
        unsigned    suw : 12;
        unsigned    rid : 8;
    };
    uint32_t     scis1;
} SU_SCIS1;

// 0x3n
typedef union
{
    struct
    {
        unsigned    ssize : 16;
        unsigned    dontcare : 16;
    };
    uint32_t     hex;
} SU_TS0;

typedef union
{
    struct
    {
        unsigned    tsize : 16;
        unsigned    dontcare : 16;
    };
    uint32_t     hex;
} SU_TS1;

// 0x40
typedef union
{
    struct
    {
        unsigned    enable : 1;
        unsigned    func : 3;
        unsigned    mask : 1;
    };
    uint32_t     hex;
} PE_ZMODE;

// 0x41
typedef union
{
    struct
    {
        unsigned    blend_en : 1;
        unsigned    logop_en : 1;
        unsigned    dither_en : 1;
        unsigned    col_mask : 1;
        unsigned    alpha_mask : 1;
        unsigned    dfactor : 3;
        unsigned    sfactor : 3;
        unsigned    blebdop : 1;
        unsigned    logop : 12;
        unsigned    rsrv : 8;
    };
    uint32_t     hex;
} ColMode0;

// 0x42
typedef union
{
    struct
    {
        unsigned    const_alpha_en : 1;
        unsigned    rsrv : 7;
        unsigned    const_alpha : 8;
        unsigned    rid : 16;
    };
    uint32_t     hex;
} ColMode1;

// local light descriptor
typedef struct
{
    uint32_t     rsrv[3];            // reserved, dont use !
    Color   color;              // light source color
    float   a[3], k[3];         // attenuation constants
    float   pos[3], dir[3];     // light position / direction
} LightObj;

// color channel descriptor
typedef union
{
    struct
    {
        unsigned    MatSrc : 1;         // 0: use register, 1: use vertex color
        unsigned    LightFunc : 1;      // 0: Use 1.0, 1: Use Illum0
        unsigned    Light0 : 1;         // 1: use light
        unsigned    Light1 : 1;         // 1: use light
        unsigned    Light2 : 1;         // 1: use light
        unsigned    Light3 : 1;         // 1: use light
        unsigned    AmbSrc : 1;         // 0: use register, 1: use vertex color
        unsigned    DiffuseAtten : 2;   // 0: Use 1.0, 1: N.L signed, 2: N.L clamped
        unsigned    Atten : 1;          // 1: atten enable
        unsigned    AttenSelect : 1;    // 0: specular, 1: spotlight
        unsigned    Light4 : 1;         // 1: use light
        unsigned    Light5 : 1;         // 1: use light
        unsigned    Light6 : 1;         // 1: use light
        unsigned    Light7 : 1;         // 1: use light
        unsigned    rsrv : 17;
    };
    uint32_t             Chan;
} ColorChan;

// matrix index A
typedef union
{
    struct
    {
        unsigned    pos : 6;
        unsigned    tex0 : 6;
        unsigned    tex1 : 6;
        unsigned    tex2 : 6;
        unsigned    tex3 : 6;
        unsigned    rsrv : 2;
    };
    uint32_t     matidx;
} MATIDX_A;

// matrix index B
typedef union
{
    struct
    {
        unsigned    tex4 : 6;
        unsigned    tex5 : 6;
        unsigned    tex6 : 6;
        unsigned    tex7 : 6;
        unsigned    rsrv : 8;
    };
    uint32_t     matidx;
} MATIDX_B;

// Array name for ArrayBase and ArrayStride

enum class ArrayId
{
    Pos = 0,
    Nrm,
    Color0,
    Color1,
    Tex0Coord,
    Tex1Coord,
    Tex2Coord,
    Tex3Coord,
    Tex4Coord,
    Tex5Coord,
    Tex6Coord,
    Tex7Coord,

    // Used by XF_IndexLoadRegA/B/C/D commands

    IndexRegA,
    IndexRegB,
    IndexRegC,
    IndexRegD,

    Max,
};

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// register memory space description

typedef struct
{
    MATIDX_A    matidxA;                // 0x20
    MATIDX_B    matidxB;                // 0x30
    unsigned    posidx, texidx[8];      // pos / tex index
    VCD_LO      vcdLo;                  // 0x50
    VCD_HI      vcdHi;                  // 0x60
    VAT_A       vatA[8];                // 0x70...0x77
    VAT_B       vatB[8];                // 0x80...0x87
    VAT_C       vatC[8];                // 0x90...0x97
    uint8_t          *arbase[(size_t)ArrayId::Max];  // 0xA0...0xAB
    uint32_t         arstride[(size_t)ArrayId::Max]; // 0xB0...0xBB
} CPMemory;

typedef struct
{
    GenMode     genmode;                // 0x00
    SU_SCIS0    scis0;                  // 0x20
    SU_SCIS1    scis1;                  // 0x21
    SU_TS0      ssize[8];               // 0x3n
    SU_TS1      tsize[8];               // 0x3n
    PE_ZMODE    zmode;                  // 0x40
    ColMode0    cmode0;                 // 0x41
    ColMode1    cmode1;                 // 0x42
    uint16_t    tokint;                 // 0x48
    LoadTlut0   loadtlut0;              // 0x64
    LoadTlut1   loadtlut1;              // 0x65
    TexMode0    texmode0[8];            // 0x80-0x83, 0xA0-0xA3
    TEXIMAGE0   teximg0[8];             // 0x88-0x8B, 0xA8-0xAB
    TEXIMAGE3   teximg3[8];             // 0x94-0x97, 0xB4-0xB7
    SetTlut     settlut[8];             // 0x98-0x9B, 0xB8-0xBB
    BOOL        valid[4][8];
} BPMemory;

typedef struct
{
    float       posmtx[64][4];          // 0x0000...0x03FF
    float       nrmmtx[32][3];          // 0x0400...0x04FF
    float       postmtx[32][3];         // 0x0500...0x05FF
    unsigned    posidx, texidx[8];      // pos index, tex index
    LightObj    light[8];               // 0x0600...0x07FF
    uint32_t         numcol;                 // 0x1009
    Color       ambient[2];             // 0x100A, 0x100B
    Color       material[2];            // 0x100C, 0x100D
    ColorChan   color[2];               // 0x100E, 0x100F
    BOOL        colmask[8][2];          // light color mask
    ColorChan   alpha[2];               // 0x1010, 0x1011
    BOOL        amask[8][2];            // light alpha mask
    uint32_t         dualtex;                // 0x1012
    MATIDX_A    matidxA;                // 0x1018
    MATIDX_B    matidxB;                // 0x1019
    float       vp_scale[3], vp_offs[3];// 0x101A...0x101F
    uint32_t         numtex;                 // 0x103F
    TexGen      texgen[8];              // 0x1040...0x1047
    DualTex     dual[8];                // 0x1050...0x1057
} XFMemory;

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// interface to application

// export register tables
extern  CPMemory  cpRegs;
extern  BPMemory  bpRegs;
extern  XFMemory  xfRegs;

extern  uint8_t   *RAM;
#define RAMMASK   0x03FFFFFF

// registers loading (using fifo writes)
void    loadCPReg(size_t index, uint32_t value);
void    loadBPReg(size_t index, uint32_t value);
void    loadXFRegs(size_t startIndex, size_t amount, GX_FromFuture::FifoProcessor* fifo);

extern  uint32_t cpLoads, bpLoads, xfLoads;
