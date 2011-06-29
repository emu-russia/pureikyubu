// compressed GCMs reading.
// format can be detected by "GCM_CMPR" string at the beginning of file.
#include "DVD.h"

#include "../zlib/zlib.h"
#pragma comment(lib, "../zlib/zlibstat.lib")

// local data
static  FILE*   gcmp;       // current file
static  s32     gcmpsize;   // size of disk image
static  s32     seekval;    // DVD seek
static  u32*    marktbl;    // 44555 entries
static  u8*     dvdbuf;     // 16 sectors

// ---------------------------------------------------------------------------

BOOL GCMPSelectFile(char *file)
{
    // open GCM file
    gcmp = fopen(file, "rb");
    if(!gcmp) return FALSE;

    // allocate mark table
    marktbl = (u32 *)malloc(44555 * 4);
    if(marktbl == NULL)
    {
        GCMPClose();
        return FALSE;
    }

    // load marktable
    fseek(gcmp, 16, SEEK_SET);
    int red = fread(marktbl, 44555 * 4, 1, gcmp);

    // count compressed image size
    gcmpsize = 0;
    for(int i=0; i<44555; i++)
    {
        if(marktbl[i] == 0) break;
        gcmpsize += 16 * 2048;
    }

    // allocate dvd buffer
    dvdbuf = (u8 *)malloc(16 * 2048);
    if(dvdbuf == NULL)
    {
        GCMPClose();
        return FALSE;
    }
    memset(dvdbuf, 0, 16 * 2048);

    // reset DVD seek
    seekval = 0;

    return TRUE;
}

void GCMPSeek(s32 position)
{
    seekval = position;
}

#define BLK_SZ  (16 * 2048)

void GCMPRead(u8 *buf, s32 length)
{
    if(gcmp)
    {
        // out of DVD
        if(seekval >= DVD_SIZE)
        {
            memset(buf, 0, length);     // fill by zeroes
            seekval += length;
            return;
        }

        // GCM files can be less than 1.4 GB,
        // so just return zeroes, when seek is out of file
        if(seekval >= gcmpsize)
        {
            memset(buf, 0, length);     // fill by zeroes
            seekval += length;
            return;
        }

        // wrap, if seek is near to out of DVD
        if( (seekval + length) >= DVD_SIZE)
        {
            length = DVD_SIZE - seekval;
        }

        // wrap, if seek is near to out of file
        if( (seekval + length) >= gcmpsize)
        {
            length = gcmpsize - seekval;
        }

        // decode and read data
        while(length > 0)
        {
            u32 mark, mask;
            u32 blk = (u32)seekval / BLK_SZ;
            u32 ofs = (u32)seekval % BLK_SZ;

            mark = marktbl[blk];
            mask = mark >> 31;
            mark &= ~(1 << 31);

            if(mark == 0)       // eof
            {
                memset(buf, 0, length);
                seekval += length;
                return;
            }

            // read 16 sectors
            fseek(gcmp, mark, SEEK_SET);

            // decompress, if need
            if(!mask)       // mask = 0, if block is compressed
            {
                uLong sourceLen, destLen = BLK_SZ;
                u8 workbuf[BLK_SZ];
                fread(&sourceLen, sizeof(sourceLen), 1, gcmp);
                if(dvd.frdm) fread(workbuf, sourceLen, 1, gcmp);
                else fread(workbuf, 1, sourceLen, gcmp);
                uncompress(dvdbuf, &destLen, workbuf, sourceLen);
            }
            else
            {
                if(dvd.frdm) fread(dvdbuf, BLK_SZ, 1, gcmp);
                else fread(dvdbuf, 1, BLK_SZ, gcmp);
            }

            if((ofs + length) >= BLK_SZ)
            {
                int bytes = BLK_SZ - ofs;
                memcpy(buf, &dvdbuf[ofs], bytes);
                buf     += bytes;
                seekval += bytes;
                length  -= bytes;
            }
            else
            {
                memcpy(buf, &dvdbuf[ofs], length);
                seekval += length;
                break;
            }
        }
    }
    else memset(buf, 0, length);
}

void GCMPClose()
{
    // close GMP file (if opened)
    if(gcmp)
    {
        fclose(gcmp);
        gcmp = NULL;
    }

    // destroy mark table
    if(marktbl)
    {
        free(marktbl);
        marktbl = NULL;
    }

    // destroy dvd buffer
    if(dvdbuf)
    {
        free(dvdbuf);
        dvdbuf = NULL;
    }
}
