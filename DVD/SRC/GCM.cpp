// simple GCM reading.
// format can be detected only by file extension.
#include "DVD.h"

// local data
static FILE*    gcm_file;       // selected GCM file
static int      gcm_size;       // size of opened file
static int      seekval;        // current DVD position

// ---------------------------------------------------------------------------

BOOL GCMSelectFile(char *file)
{
    dvd.selected = false;

    // open GCM file
    gcm_file = fopen(file, "rb");
    if(!gcm_file) return FALSE;

    // get file size
    fseek(gcm_file, 0, SEEK_END);
    gcm_size = ftell(gcm_file);
    fseek(gcm_file, 0, SEEK_SET);

    // protect from damaged GCMs
    if(gcm_size < DVD_APPLDR_OFFSET)
    {
        GCMClose();
        return FALSE;
    }

    // reset position
    seekval = 0;

    dvd.selected = true;

    return TRUE;
}

void GCMSeek(int position)
{
    seekval = position;
}

void GCMRead(uint8_t*buf, int length)
{
    if(gcm_file)
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
        if(seekval >= gcm_size)
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
        if( (seekval + length) >= gcm_size)
        {
            length = gcm_size - seekval;
        }

        // read data
        if(length)
        {
            fseek(gcm_file, seekval, SEEK_SET);
            if(dvd.frdm) fread(buf, length, 1, gcm_file);
            else fread(buf, 1, length, gcm_file);
            seekval += length;
        }
    }
    else memset(buf, 0, length);        // fill by zeroes
}

void GCMClose()
{
    // close GCM file (if opened)
    if(gcm_file)
    {
        fclose(gcm_file);
        gcm_file = NULL;
    }
}
