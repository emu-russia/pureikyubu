// External interface
#include "pch.h"

using namespace Debug;

uint8_t* RAM;
HINSTANCE   hPlugin;
HWND hwndMain;

static bool gxOpened = false;
extern int     VtxSize[8];

// Dump CommandProcessor VCD/VAT settings
static Json::Value* DumpVat(std::vector<std::string>& args)
{
    // VCD

    static char* typname[] = { "NONE", "DIRECT", "INDEX8", "INDEX16" };

    Report(Channel::Norm, "VCD:\n");
    Report(Channel::Norm, "pmidx: %i\n", cpRegs.vcdLo.pmidx);
    Report(Channel::Norm, "t0midx: %i\n", cpRegs.vcdLo.t0midx);
    Report(Channel::Norm, "t1midx: %i\n", cpRegs.vcdLo.t1midx);
    Report(Channel::Norm, "t2midx: %i\n", cpRegs.vcdLo.t2midx);
    Report(Channel::Norm, "t3midx: %i\n", cpRegs.vcdLo.t3midx);
    Report(Channel::Norm, "t4midx: %i\n", cpRegs.vcdLo.t4midx);
    Report(Channel::Norm, "t5midx: %i\n", cpRegs.vcdLo.t5midx);
    Report(Channel::Norm, "t6midx: %i\n", cpRegs.vcdLo.t6midx);
    Report(Channel::Norm, "t7midx: %i\n", cpRegs.vcdLo.t7midx);
    Report(Channel::Norm, "pos: %s\n", typname[cpRegs.vcdLo.pos]);
    Report(Channel::Norm, "nrm: %s\n", typname[cpRegs.vcdLo.nrm]);
    Report(Channel::Norm, "col0: %s\n", typname[cpRegs.vcdLo.col0]);
    Report(Channel::Norm, "col1: %s\n", typname[cpRegs.vcdLo.col1]);
    Report(Channel::Norm, "tex0: %s\n", typname[cpRegs.vcdHi.tex0]);
    Report(Channel::Norm, "tex1: %s\n", typname[cpRegs.vcdHi.tex1]);
    Report(Channel::Norm, "tex2: %s\n", typname[cpRegs.vcdHi.tex2]);
    Report(Channel::Norm, "tex3: %s\n", typname[cpRegs.vcdHi.tex3]);
    Report(Channel::Norm, "tex4: %s\n", typname[cpRegs.vcdHi.tex4]);
    Report(Channel::Norm, "tex5: %s\n", typname[cpRegs.vcdHi.tex5]);
    Report(Channel::Norm, "tex6: %s\n", typname[cpRegs.vcdHi.tex6]);
    Report(Channel::Norm, "tex7: %s\n", typname[cpRegs.vcdHi.tex7]);

    // VATs

    for (int i = 0; i < 8; i++)
    {
        static char* fmtname[] = { "U8", "S8", "U16", "S16", "F32", "Reserved5", "Reserved6", "Reserved7" };
        static char* colname[] = { "RGB565", "RGB8", "RGBX8", "RGBA4", "RGBA6", "RGBA8", "Reserved6", "Reserved7" };

        Report(Channel::Norm, "\n");
        Report(Channel::Norm, "VAT[%i]:\n", i);

        Report(Channel::Norm, "pos: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatA[i].poscnt, fmtname[cpRegs.vatA[i].posfmt], cpRegs.vatA[i].posshft);
        Report(Channel::Norm, "nrm: cnt:%i, fmt:%s\n", cpRegs.vatA[i].nrmcnt, fmtname[cpRegs.vatA[i].nrmfmt]);
        Report(Channel::Norm, "col0: cnt:%i, fmt:%s\n", cpRegs.vatA[i].col0cnt, colname[cpRegs.vatA[i].col0fmt]);
        Report(Channel::Norm, "col1: cnt:%i, fmt:%s\n", cpRegs.vatA[i].col1cnt, colname[cpRegs.vatA[i].col1fmt]);
        Report(Channel::Norm, "tex0: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatA[i].tex0cnt, fmtname[cpRegs.vatA[i].tex0fmt], cpRegs.vatA[i].tex0shft);
        Report(Channel::Norm, "tex1: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatB[i].tex1cnt, fmtname[cpRegs.vatB[i].tex1fmt], cpRegs.vatB[i].tex1shft);
        Report(Channel::Norm, "tex2: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatB[i].tex2cnt, fmtname[cpRegs.vatB[i].tex2fmt], cpRegs.vatB[i].tex2shft);
        Report(Channel::Norm, "tex3: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatB[i].tex3cnt, fmtname[cpRegs.vatB[i].tex3fmt], cpRegs.vatB[i].tex3shft);
        Report(Channel::Norm, "tex4: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatB[i].tex4cnt, fmtname[cpRegs.vatB[i].tex4fmt], cpRegs.vatC[i].tex4shft);
        Report(Channel::Norm, "tex5: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatC[i].tex5cnt, fmtname[cpRegs.vatC[i].tex5fmt], cpRegs.vatC[i].tex5shft);
        Report(Channel::Norm, "tex6: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatC[i].tex6cnt, fmtname[cpRegs.vatC[i].tex6fmt], cpRegs.vatC[i].tex6shft);
        Report(Channel::Norm, "tex7: cnt:%i, fmt:%s, shft:%i\n", cpRegs.vatC[i].tex7cnt, fmtname[cpRegs.vatC[i].tex7fmt], cpRegs.vatC[i].tex7shft);

        Report(Channel::Norm, "bytedeq: %i, nrmidx3: %i, vcacheEnch: %i\n", cpRegs.vatA[i].bytedeq, cpRegs.vatA[i].nrmidx3, cpRegs.vatB[i].vcache);
        Report(Channel::Norm, "VertexSize (calculated): %i bytes\n", VtxSize[i]);
    }

    return nullptr;
}

void DolwinVideoReflector()
{
    JDI::Hub.AddCmd("DumpVat", DumpVat);
}

long GXOpen(HWConfig* config, uint8_t * ramPtr)
{
    if (gxOpened)
        return 1;

    BOOL res;

    hPlugin = GetModuleHandle(NULL);
    hwndMain = (HWND)config->renderTarget;

    RAM = ramPtr;

    res = GL_LazyOpenSubsystem(hwndMain);
    assert(res);

    // vertex programs extension
    //SetupVertexShaders();
    //ReloadVertexShaders();

    // reset pipeline
    frame_done = true;

    // flush texture cache
    TexInit();

    gxOpened = true;

    JDI::Hub.AddNode(DOLWIN_VIDEO_JDI_JSON, DolwinVideoReflector);

    return true;
}

void GXClose()
{
    if (!gxOpened)
        return;

    GL_CloseSubsystem();

    TexFree();

    JDI::Hub.RemoveNode(DOLWIN_VIDEO_JDI_JSON);

    gxOpened = false;
}
