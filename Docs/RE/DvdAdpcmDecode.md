# GC DVD ADPCM Decoder

Based on dtkmake reversing.

## Get next 32 Byte chunk

```c++
    uint8_t adpcmBuf[32];

    fread ( adpcmBuf, 1, 32, f);
```

## Check

```c++

{
    if ( buf[0] != buf[2] )
    {
        // invalid ADPCM file
    }

    if ( buf[1] != buf[3] )
    {
        // invalid ADPCM file
    }
}
```

## Decode 28 bytes of samples into 0x70 bytes PCM

```c++

uint8_t * adpcmData = &adpcmBuf[4];

typedef struct _LRSample
{
    uint16_t l;
    uint16_t r;
} LRSample;

LRSample pcmData[28];

for (int i=0; i<28; i++)
{
    pcmData[i].l = DecodeLeftSample ( adpcmData[i] & 0xF, adpcmBuf[0], 0);
    pcmData[i].r = DecodeRightSample ( adpcmData[i] >> 4, adpcmBuf[1], 0);
}
```

### Decode Sample

```c++
typedef int Nibble;

int16_t DecodeLeftSample ( int arg_0, int arg_4, bool init)
{
    int16_t res;

    if (init)
    {
        Yn[0] = 0;           // Для Right sample просто две других аналогичных переменных
        Yn[1] = 0;
        res = 0;
    }
    else
    {
        Nibble var_4 = arg_4 >> 4;
        Nibble var_8 = arg_4 & 0xF;

        int32_t var_18 = MulSomething(var_4, Yn[0], Yn[1]);
        int16_t var_C = Shifts1(arg_0, var_8);
        int32_t var_14 = Shifts2(var_C, var_18);
        res = Clamp(var_14);

        Yn[1] = Yn[0];
        Yn[0] = var_14;
    }

    return res;
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

```

## Write chunk into WAV

```c++
    fwrite ( &pcmData, 1, sizeof(pcmData), wav);

```
