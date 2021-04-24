#include "pch.h"

int main(int argc, char **argv)
{
    // Deploy Dsp Core

    DSP::Dsp16* core = new DSP::Dsp16();
    assert(core);

    // Init adpcm decoder

    core->Accel.AdpcmCoef[0] = 0x00bf;
    core->Accel.AdpcmCoef[1] = 0xffc8;
    core->Accel.AdpcmCoef[2] = 0x0661;
    core->Accel.AdpcmCoef[3] = 0xff43;
    core->Accel.AdpcmCoef[4] = 0x02b9;
    core->Accel.AdpcmCoef[5] = 0x037d;
    core->Accel.AdpcmCoef[6] = 0x0a35;
    core->Accel.AdpcmCoef[7] = 0xfd14;

    core->Accel.AdpcmCoef[8] = 0x03ca;
    core->Accel.AdpcmCoef[9] = 0x000e;
    core->Accel.AdpcmCoef[10] = 0x0732;
    core->Accel.AdpcmCoef[11] = 0x0018;
    core->Accel.AdpcmCoef[12] = 0x0499;
    core->Accel.AdpcmCoef[13] = 0x02ee;
    core->Accel.AdpcmCoef[14] = 0x0cf9;
    core->Accel.AdpcmCoef[15] = 0xfad5;

    core->Accel.AdpcmGan = 0;
    core->Accel.AdpcmYn1 = 0;
    core->Accel.AdpcmYn2 = 0;

    // Load Adpcm buffer

    std::vector<uint8_t> adpcmData = Util::FileLoad(argv[1]);

    // Create output file

    FILE* out;

    fopen_s(&out, "out.bin", "wb");
    assert(out);

    // Decode

    uint8_t* ptr = adpcmData.data();
    size_t byteCounter = 0;

    size_t adpcmDataSize = adpcmData.size();

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
    delete core;
}
