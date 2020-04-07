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

int16_t DecodeLeftSample ( uint16_t arg_0, int16_t arg_4, bool init)
{
	int16_t res;

	if (init)
	{
		dword_429028 = 0; 			// Для Right sample просто две других аналогичных переменных
		dword_429030 = 0;
		res = 0;
	}
	else
	{
		var_4 = arg_4 >> 4;
		var_8 = arg_4 & 0xF;

		var_18 = MulSomething ( var_4, dword_429028, dword_429030); 	//ax 
		var_C = Shifts1 (arg_0, var_8);
		var_14 = Shifts2 (var_C, var_18);
		res = Clamp (var_14);

		dword_429030 = dword_429028;
		dword_429028 = var_14;
	}

	return res;
}

// Чё-то умножаем
int MulSomething (arg_0, arg_4, arg_8 )
{
	var_10 = arg_0;

	switch (var_10)
	{
		case 0:
			var_4 = 0;
			var_8 = 0;
			break;
		case 1:
			var_4 = 0x3c;
			var_8 = 0;
			break;
		case 2:
			var_4 = 0x73;
			var_8 = 0xFFCC;
			break;
		case 3:
			var_4 = 0x62;
			var_8 = 0xFFC9;
			break;
	}

	// Нужно смотреть мануал, я всегда забываю где хранятся результаты умножения
	movsx   edx, [ebp+var_4]
	imul    edx, [ebp+arg_4]
	movsx   eax, [ebp+var_8]
	imul    eax, [ebp+arg_8]

	var_C = (edx + eax + 0x20) >> 6;

	var_C = max ( -0x200000‬, min (var_C, 0x1FFFFF));

	return var_C;
}

// Кручу-верчу-наебать-хочу 1
int16_t Shifts1 (int arg_0, int8_t arg_4)
{
	int32_t var_4 = arg_0 << 12;
	int32_t edx = var_4 >> arg_4;
	return dx;
}

// Кручу-верчу-наебать-хочу 2
int Shifts2 (int16_t arg_0, arg_4)
{
	return (arg_0 << 6) + arg_4;
}

// Clamp
int16_t Clamp (arg_0)
{
	var_8 = arg_0 >> 6; 		// sar

	if (var_8 >= 0xFFFF8000)
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

	return var_4;
}


```

## Write chunk into WAV

```c++
	fwrite ( &pcmData, 1, sizeof(pcmData), wav);

```
