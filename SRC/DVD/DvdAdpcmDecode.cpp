
#include "pch.h"

int YnLeft[2], YnRight[2];

void DvdAudioInitDecoder()
{
	YnLeft[0] = YnLeft[1] = 0;
	YnRight[0] = YnRight[1] = 0;
}

int32_t MulSomething(int arg_0, int32_t arg_4, int32_t arg_8)
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

    int32_t edx = var_4 * arg_4;
    int32_t eax = var_8 * arg_8;

    int32_t var_C = (edx + eax + 32) >> 6;
    var_C = max( (-0x200000), min(var_C, 0x1FFFFF) );
   
    return var_C;
}

int16_t Shifts1(int arg_0, int arg_4)
{
    int32_t var_4 = (int32_t)arg_0 << 12;
    int32_t edx = var_4 >> arg_4;
    return (int16_t)edx;
}

int32_t Shifts2(int16_t arg_0, int32_t arg_4)
{
    return ((int32_t)arg_0 << 6) + arg_4;
}

// Clamp
int16_t Clamp(int32_t arg_0)
{
    int32_t var_8 = arg_0 >> 6;
    int32_t var_4;

    if (var_8 >= -0x8000)
    {
        if (var_8 <= 0x7FFF)
        {
            var_4 = var_8;
        }
        else
        {
            var_4 = 0x7FFF;
        }
    }
    else
    {
        var_4 = 0x8000;
    }

    return (int16_t)var_4;
}

int16_t DecodeSample(int arg_0, int arg_4, int Yn[2])
{
    int16_t res;

    int var_4 = arg_4 >> 4;
    int var_8 = arg_4 & 0xF;

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

    for (int i = 0; i < 28; i++)
    {
        pcmBuffer[2 * i] = _byteswap_ushort(DecodeSample(adpcmData[i] & 0xF, adpcmBuffer[0], YnLeft));
        pcmBuffer[2 * i + 1] = _byteswap_ushort(DecodeSample(adpcmData[i] >> 4, adpcmBuffer[1], YnRight));
    }
}
