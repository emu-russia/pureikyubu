/* Dolphin executable file format specifications. */

#include <dolphin/types.h>

#ifndef __DOLFORMAT_H__
#define __DOLFORMAT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DOL_MAX_TEXT 7
#define DOL_MAX_DATA 11

typedef struct DolImage
{
    u8      *textData[DOL_MAX_TEXT];    /* Section offset from begin of file. */
    u8      *dataData[DOL_MAX_DATA];
    u32     text[DOL_MAX_TEXT];         /* Virtual address to load section. */
    u32     data[DOL_MAX_DATA];
    u32     textLen[DOL_MAX_TEXT];      /* Size of section in bytes. */
    u32     dataLen[DOL_MAX_DATA];
    u32     bss;
    u32     bssLen;
    u32     entry;                      /* Program entrypoint. */
    u8      padding[28];
} DolImage;

#ifdef __cplusplus
}
#endif

#endif
