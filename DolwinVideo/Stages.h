// define fifo pipeline callbacks

//
// fifo stages set
// attr[TYPE][CNT][FMT]
//

static u8 *(__fastcall *posattr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // XY
    { NULL, NULL, NULL, NULL, NULL }    // XYZ
    },
        
    {   // direct
    { pos_xy_u8_d, pos_xy_s8_d, pos_xy_u16_d, pos_xy_s16_d, pos_xy_f32_d },   // XY
    { pos_xyz_u8_d, pos_xyz_s8_d, pos_xyz_u16_d, pos_xyz_s16_d, pos_xyz_f32_d }    // XYZ
    },

    {   // x8
    { pos_xy_u8_x8, pos_xy_s8_x8, NULL, pos_xy_s16_x8, NULL },   // XY
    { NULL, NULL, NULL, pos_xyz_s16_x8, pos_xyz_f32_x8 }    // XYZ
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // XY
    { NULL, NULL, NULL, pos_xyz_s16_x16, pos_xyz_f32_x16 }    // XYZ
    }

    // U8    S8   U16   S16   F32
};

static u8 *(__fastcall *nrmattr[4][3][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // XYZ
    { NULL, NULL, NULL, NULL, NULL },   // NBT
    { NULL, NULL, NULL, NULL, NULL }    // NBT3
    },

    {   // direct
    { nrm_xyz_u8_d, nrm_xyz_s8_d, nrm_xyz_u16_d, nrm_xyz_s16_d, nrm_xyz_f32_d },   // XYZ
    { NULL, NULL, NULL, NULL, nrm_nbt_f32_d },   // NBT
    { NULL, NULL, NULL, NULL, NULL }    // NBT3
    },

    {   // x8
    { nrm_xyz_u8_x8, nrm_xyz_s8_x8, NULL, nrm_xyz_s16_x8, nrm_xyz_f32_x8 },   // XYZ
    { NULL, NULL, NULL, nrm_nbt_s16_x8, NULL },   // NBT
    { NULL, NULL, NULL, NULL, NULL }    // NBT3
    },

    {   // x16
    { NULL, NULL, NULL, nrm_xyz_s16_x16, nrm_xyz_f32_x16 },   // XYZ
    { NULL, NULL, NULL, NULL, NULL },   // NBT
    { NULL, NULL, NULL, NULL, NULL }    // NBT3
    },

    // U8    S8   U16   S16   F32
};

// ---------------------------------------------------------------------------

static u8 *(__fastcall *col0attr[4][2][6])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    },
        
    {   // direct
    { clr0_rgb_rgb565_d, clr0_rgb_rgb8_d, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, clr0_rgba_rgba4_d, NULL, clr0_rgba_rgba8_d }      // RGBA
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL, clr0_rgb_rgba8_x8 },     // RGB
    { NULL, NULL, NULL, NULL, NULL, clr0_rgba_rgba8_x8 }      // RGBA
    },

    {   // x16
    { NULL, clr0_rgb_rgb8_x16, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, clr0_rgba_rgba8_x16 }      // RGBA
    }

//  RGB565  RGB8  RGBX8 RGBA4 RGBA6 RGBA8
};

static u8 *(__fastcall *col1attr[4][2][6])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    },
        
    {   // direct
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL, NULL },     // RGB
    { NULL, NULL, NULL, NULL, NULL, NULL }      // RGBA
    }

//  RGB565  RGB8  RGBX8 RGBA4 RGBA6 RGBA8
};

// ---------------------------------------------------------------------------

static u8 *(__fastcall *tex0attr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { tex0_st_u8_d, tex0_st_s8_d, tex0_st_u16_d, tex0_st_s16_d, tex0_st_f32_d }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, tex0_st_s8_x8, tex0_st_u16_x8, tex0_st_s16_x8, tex0_st_f32_x8 }    // ST
    },

    {   // x16
    { tex0_s_u8_x16, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, tex0_st_u16_x16, tex0_st_s16_x16, tex0_st_f32_x16 }    // ST
    },

    // U8    S8   U16   S16   F32
};

static u8 *(__fastcall *tex1attr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { tex1_s_u8_x8, tex1_s_s8_x8, tex1_s_u16_x8, tex1_s_s16_x8, tex1_s_f32_x8 },   // S
    { tex1_st_u8_x8, tex1_st_s8_x8, tex1_st_u16_x8, tex1_st_s16_x8, tex1_st_f32_x8 }    // ST
    },

    {   // x16
    { tex1_s_u8_x16, tex1_s_s8_x16, tex1_s_u16_x16, tex1_s_s16_x16, tex1_s_f32_x16 },   // S
    { tex1_st_u8_x16, tex1_st_s8_x16, tex1_st_u16_x16, tex1_st_s16_x16, tex1_st_f32_x16 }    // ST
    },

    // U8    S8   U16   S16   F32
};

static u8 *(__fastcall *tex2attr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

static u8 *(__fastcall *tex3attr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

static u8 *(__fastcall *tex4attr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

static u8 *(__fastcall *tex5attr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

static u8 *(__fastcall *tex6attr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};

static u8 *(__fastcall *tex7attr[4][2][5])(u8 *) = {
    {   // none
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // direct
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x8
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    {   // x16
    { NULL, NULL, NULL, NULL, NULL },   // S
    { NULL, NULL, NULL, NULL, NULL }    // ST
    },

    // U8    S8   U16   S16   F32
};
