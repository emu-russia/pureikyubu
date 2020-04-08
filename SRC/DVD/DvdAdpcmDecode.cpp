
#include "pch.h"

int32_t YnLeft[2], YnRight[2];

typedef int Nibble;

void DvdAudioInitDecoder()
{
	YnLeft[0] = YnLeft[1] = 0;
	YnRight[0] = YnRight[1] = 0;
}

int32_t MulSomething(Nibble arg_0, int32_t arg_4, int32_t arg_8)
{
    int16_t var_4 = 0;
    int16_t var_8 = 0;

    switch (arg_0)
    {
        case 0:
            var_4 = 0;
            var_8 = 0;
            break;
        case 1:
            var_4 = 60;
            var_8 = 0;
            break;
        case 2:
            var_4 = 115;
            var_8 = -52;
            break;
        case 3:
            var_4 = 98;
            var_8 = -55;
            break;
    }

    int32_t edx = (int32_t)var_4 * arg_4;
    int32_t eax = (int32_t)var_8 * arg_8;

    int32_t var_C = (edx + eax + 32) >> 6;
    return max(-0x200000, min(var_C, 0x1FFFFF) );
}

int16_t Shifts1(Nibble arg_0, Nibble arg_4)
{
    int16_t var_4 = (int16_t)arg_0 << 12;
    return var_4 >> arg_4;
}

int32_t Shifts2(int16_t arg_0, int32_t arg_4)
{
    return ((int32_t)arg_0 << 6) + arg_4;
}

// Clamp
uint16_t Clamp(int32_t arg_0)
{
    int32_t var_8 = arg_0 >> 6;
    return (uint16_t)max(-0x8000, min(var_8, 0x7FFF));
}

uint16_t DecodeSample(Nibble arg_0, uint8_t arg_4, int Yn[2])
{
    uint16_t res;

    Nibble var_4 = arg_4 >> 4;
    Nibble var_8 = arg_4 & 0xF;

    int32_t var_18 = MulSomething(var_4, Yn[0], Yn[1]);
    int16_t var_C = Shifts1(arg_0, var_8);
    int32_t var_14 = Shifts2(var_C, var_18);
    res = Clamp(var_14);

    Yn[1] = Yn[0];
    Yn[0] = var_14;

    return res;
}

void DvdAudioDecode(uint8_t adpcmBuffer[32], uint16_t pcmBuffer[2 * 28])
{
    uint8_t* adpcmData = &adpcmBuffer[4];

    if (!(adpcmBuffer[0] == adpcmBuffer[2] && adpcmBuffer[1] == adpcmBuffer[3]))
    {
        memset(pcmBuffer, 0, sizeof(pcmBuffer));
        return;
    }

    for (int i = 0; i < 28; i++)
    {
        pcmBuffer[2 * i] = DecodeSample(adpcmData[i] & 0xF, adpcmBuffer[0], YnLeft);
        pcmBuffer[2 * i + 1] = DecodeSample(adpcmData[i] >> 4, adpcmBuffer[1], YnRight);
    }
}
