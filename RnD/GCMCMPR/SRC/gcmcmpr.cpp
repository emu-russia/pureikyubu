// Dolwin GCM compression tool.

/*/ ---------------------------------------------------------------------------

    Where 'GMP' comes from?
    -----------------------

    'GMP' is short for 'Gamecube Master data Packed', or simply 'GCM comPressed'.

    GMP compression details:
    ------------------------

    Whole DVD image is subdivided on blocks. Every block equals 16 DVD sectors.
    So maximum number of blocks is 44555 (DVD size / 16 * 2048), but there are
    exist smaller DVDs - we assume zeroes on upper area.

    Algorithm is using lookup table, to quickly find compressed block, by its
    disk offset. This table also referred as "mark table". This table is 
    placed at the beginning of GMP file.

    Every block is compressed individualy. Here is short description of
    compression :

        Step 1: get next 16 sectors (block) from DVD image
        Step 2: compress block by ZLIB
        Step 3: put DVD offset record in mark table (save block offset)
        Step 4: put compressed data in GMP file
        Step 5: adjust offset on next 16 compressed sectors

    If compressed block is greater, than raw data, put it unchanged in GMP;
    Special bit in mark table is used to show that data is uncompressed.

    Decompression is quite simple. We are getting block offset from mark
    table, then decompressing that block and returning data to DVD hardware.

    Quick file format :

        [GMP ID] - 16 bytes, containing ID string ("GCM_CMPR" and zeroes);
        [MARK TABLE] - 44555 DWORDs in little-endian Intel format (offsets);
                       last block for small DVDs is marked as 0x00000000.
        [COMPRESSED DVD DATA] - depends on compression and size of DVD.

--------------------------------------------------------------------------- /*/

#include <conio.h>
#include "dolphin.h"

#include "../zlib/zlib.h"
#pragma comment(lib, "../zlib/zlibstat.lib")

// ID for Dolwin compressed GCM file
#define GMP_ID  "GCM_CMPR"

// tool version
#define VERSION "1.0"

// forward refs
void    GCMCompress(char *infile, char *outfile);
void    GCMDecompress(char *infile, char *outfile);

// ---------------------------------------------------------------------------
// command line stuff

// show tool banner
static void banner()
{
    printf("GCMCMPR ver. " VERSION ", " __DATE__ ", ZLIB ver. %s\n", zlibVersion());
    printf("GCM compression tool for " APPNAME " " APPVER " Emulator.\n");
    printf("(C) Copyright 2003-2005, " APPNAME " Team.\n");
    printf("\n");
}

// show help, if no command line parameters.
static void help()
{
    printf("To compress   : GCMCMPR \"gcm_file\" \"gmp_file\"\n");
    printf("To decompress : GCMCMPR \"gmp_file\" \"gcm_file\"\n");
    printf("(quotes are optionally for long file names with spaces)\n");
    printf("\n");
    printf("Operation may take from 5 to 15 minutes.\n");
    printf("Compressed GCM files have .GMP extension.\n");
    //printf("Recommended to wipe GCM before compression, by GCTool 1.20.\n");
}

// return 1, if file is GMP
static BOOL is_compressed(char *filename)
{
    char buf[16];
    FILE *f = fopen(filename, "rb");
    fread(buf, 16, 1, f);
    fclose(f);
    return (strcmp(buf, GMP_ID) == 0) ? (TRUE) : (FALSE);
}

// tool entrypoint
void main(int argc, char **argv)
{
    banner();
    if(argc <= 2)
    {
        help();
        return;
    }

    char * infile = argv[1];
    char * outfile = argv[2];

    if(is_compressed(infile))
    {
        GCMDecompress(infile, outfile);
    }
    else
    {
        GCMCompress(infile, outfile);
    }

    getch();
}

// ---------------------------------------------------------------------------
// compression / decompression

#define BLK_SZ  (16 * 2048)

void GCMCompress(char *infile, char *outfile)
{
    u32 marktbl[44555], i = 0, mark, mask;
    u8  dvd_sectors[BLK_SZ], workbuf[BLK_SZ*2 + 12];
    uLong result;
    FILE *in, *out;
    s64 inSize, outSize = sizeof(marktbl) + 16;

    // files are the same ? (for stupid user)
    if(!stricmp(infile, outfile))
    {
        printf("Cannot compress file into itself!\n");
        return;
    }

    // try to open both files
    in = fopen(infile, "rb");
    if(in == NULL)
    {
        printf("Cannot open input file : %s\n", infile);
        return;
    }
    out = fopen(outfile, "wb");
    if(out == NULL)
    {
        printf("Cannot open output file : %s\n", outfile);
        return;
    }

    // check infile - must be de-compressed
    if(is_compressed(infile) == TRUE)
    {
        printf("Input file must be decompressed : %s\n", infile);
        return;
    }

    // get size of input file
    fseek(in, 0, SEEK_END);
    inSize = ftell(in);
    fseek(in, 0, SEEK_SET);

    // show info
    printf("Compressing \'%s\' -> \'%s\'\n", infile, outfile);

    // compress
    char id[16];
    memset(id, 0, 16);
    strcpy(id, GMP_ID);
    fwrite(id, 16, 1, out);
    memset(marktbl, 0, sizeof(marktbl));
    fwrite(marktbl, sizeof(marktbl), 1, out);
    mark = 16 + sizeof(marktbl);
    while(i < 44555)    // 44555 = dvd size / 16 sectors
    {
        // Step 1: read 16 dvd sectors
        if(feof(in))
        {
            marktbl[i] = 0;
            break;
        }
        fread(dvd_sectors, BLK_SZ, 1, in);

        // Step 2: compress by ZLIB
        // dvd_sectors = in data, BLK_SZ = in data size, result = out data size
        result = sizeof(workbuf);
        compress(workbuf, &result, dvd_sectors, BLK_SZ);

        // if mask bit31 = 1, keep block uncompressed
        if(result >= BLK_SZ) mask = 0x80000000;
        else mask = 0;

        // Step 3: put dvd offset record in mark table
        marktbl[i++] = mark | mask;

        // Step 4: put compressed data in file
        if(mask)
        {
            fwrite(dvd_sectors, BLK_SZ, 1, out);
            outSize += BLK_SZ;
        }
        else
        {
            fwrite(&result, 4, 1, out);
            fwrite(workbuf, result, 1, out);
            result += 4;
            outSize += result;
        }

        // Step 5: adjust mark on next 16 compressed sectors
        if(mask) mark += BLK_SZ;
        else mark += result;
        if((i % 100) == 0)
        {
            printf("\r");
            printf("block: %i / 44555", i);
            fflush(out);
        }
    }
    fclose(out);
    out = fopen(outfile, "rb+");
    fwrite(id, 16, 1, out);
    fwrite(marktbl, sizeof(marktbl), 1, out);

    // clean-up
    printf("\n%i block(s) compressed. ratio : %i%%\n", i, 100 - (s32)((outSize * 100) / inSize));
    fclose(in);
    fclose(out);
}

void GCMDecompress(char *infile, char *outfile)
{
    u32 marktbl[44555], i = 0, mark, mask;
    u8  dvd_sectors[BLK_SZ], id[16];
    FILE *in, *out;

    // files are the same ? (for stupid user)
    if(!stricmp(infile, outfile))
    {
        printf("Cannot decompress file into itself!\n");
        return;
    }

    // check infile - must be compressed
    if(is_compressed(infile) == FALSE)
    {
        printf("Input file must be compressed : %s\n", infile);
        return;
    }

    // try to open both files
    in = fopen(infile, "rb");
    if(in == NULL)
    {
        printf("Cannot open input file : %s\n", infile);
        return;
    }
    out = fopen(outfile, "wb");
    if(out == NULL)
    {
        printf("Cannot open output file : %s\n", outfile);
        return;
    }

    // show info
    printf("Decompressing \'%s\' -> \'%s\'\n", infile, outfile);

    // decompress
    fread(id, 16, 1, in);
    fread(marktbl, sizeof(marktbl), 1, in);
    while(i < 44555)    // 44555 = dvd size / 16 sectors
    {
        // Step 1: read next mark
        mark = marktbl[i++];
        mask = mark >> 31;
        if(mark == 0) break;    // end-of-file

        // Step 2: uncompress (if needed)
        fseek(in, mark & ~0x80000000, SEEK_SET);
        if(!mask)
        {
            uLong sourceLen, destLen = BLK_SZ;
            u8 workbuf[BLK_SZ];
            fread(&sourceLen, sizeof(sourceLen), 1, in);
            fread(workbuf, sourceLen, 1, in);
            uncompress(dvd_sectors, &destLen, workbuf, sourceLen);
        }
        else fread(dvd_sectors, BLK_SZ, 1, in);

        // Step 3: put 16 dvd sectors
        fwrite(dvd_sectors, BLK_SZ, 1, out);
        if((i % 100) == 0)
        {
            printf("\r");
            printf("block: %i / 44555", i);
            fflush(out);
        }
    }

    // clean-up
    printf("\n%i block(s) decompressed.\n", i);
    fclose(in);
    fclose(out);
}
