// GXTexture.c (Debug version)
//

void __GXGetTexTileShift (GXTexFmt fmt, u32 *rowTileS, u32 *colTileS)
{
    switch ( fmt )
    {
        case GX_TF_I4:
        case GX_TF_C4:
        case GX_TF_CMPR:
        case GX_CTF_R4:
        case GX_CTF_Z4:
            *rowTileS = 3;
            *colTileS = 3;
            break;

        case GX_TF_I8:
        case GX_TF_IA4:
        case GX_TF_C8:
        case GX_TF_Z8:
        case GX_CTF_RA4:
        case GX_CTF_A8:
        case GX_CTF_R8:
        case GX_CTF_G8:
        case GX_CTF_B8:
        case GX_CTF_Z8M:
        case GX_CTF_Z8L:
            *rowTileS = 3;
            *colTileS = 2;
            break;

        case GX_TF_IA8:
        case GX_TF_RGB565:
        case GX_TF_RGB5A3:
        case GX_TF_RGBA8:
        case GX_TF_C14X2:
        case GX_TF_Z16:
        case GX_TF_Z24X8:
        case GX_CTF_RA8:
        case GX_CTF_RG8:
        case GX_CTF_GB8:
        case GX_CTF_Z16L:
            *rowTileS = 2;
            *colTileS = 2;
            break;

        default:
            OSHalt ( "%s: invalid texture format", "GX" );
    }
}
