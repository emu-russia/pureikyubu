#include "pch.h"

int main(int argc, char **argv)
{
    // Deploy Dsp Core

    DSP::DspCore* core = new DSP::DspCore(nullptr);
    assert(core);

    // Init adpcm decoder

    core->Accel.AdpcmCoef[0] = 0x0136;
    core->Accel.AdpcmCoef[1] = 0xfe78;
    core->Accel.AdpcmCoef[2] = 0x06b2;
    core->Accel.AdpcmCoef[3] = 0xff09;
    core->Accel.AdpcmCoef[4] = 0x029f;
    core->Accel.AdpcmCoef[5] = 0x038d;
    core->Accel.AdpcmCoef[6] = 0x0a05;
    core->Accel.AdpcmCoef[7] = 0xfd47;
    core->Accel.AdpcmCoef[8] = 0x047d;
    core->Accel.AdpcmCoef[9] = 0xff11;
    core->Accel.AdpcmCoef[10] = 0x0742;
    core->Accel.AdpcmCoef[11] = 0x0005;
    core->Accel.AdpcmCoef[12] = 0x04ce;
    core->Accel.AdpcmCoef[13] = 0x02b7;
    core->Accel.AdpcmCoef[14] = 0x0c87;
    core->Accel.AdpcmCoef[15] = 0xfb49;

    core->Accel.AdpcmGan = 0;
    core->Accel.AdpcmYn1 = 0;
    core->Accel.AdpcmYn2 = 0;

    // Load Adpcm buffer

    size_t adpcmDataSize = 0;

    uint8_t* adpcmData = (uint8_t *)UI::FileLoad(argv[1], &adpcmDataSize);
    assert(adpcmData);

    // Create output file

    FILE* out;

    fopen_s(&out, "out.bin", "wb");
    assert(out);

    // Decode

    uint8_t* ptr = adpcmData;
    size_t byteCounter = 0;

    while (adpcmDataSize != 0)
    {
        uint16_t pcmSample = 0;

        uint8_t nextByte = *ptr++;

        if (byteCounter == 0)
        {
            core->Accel.AdpcmPds = nextByte;
        }
        else
        {
            pcmSample = _byteswap_ushort(core->DecodeAdpcm(nextByte >> 4));
            fwrite(&pcmSample, 1, sizeof(uint16_t), out);

            pcmSample = _byteswap_ushort(core->DecodeAdpcm(nextByte & 0xf));
            fwrite(&pcmSample, 1, sizeof(uint16_t), out);
        }

        adpcmDataSize--;
        byteCounter++;
        if (byteCounter >= 8)
        {
            byteCounter = 0;
        }
    }

    fclose(out);
    free(adpcmData);
    delete core;
}
