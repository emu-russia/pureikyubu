// simple GCM image reading.
#include "pch.h"

// local data
static TCHAR    gcm_filename[0x1000];
static int      gcm_size;       // size of mounted file
static int      seekval;        // current DVD position

// ---------------------------------------------------------------------------

bool GCMMountFile(const TCHAR *file)
{
    FILE* gcm_file;

    gcm_filename[0] = 0;
    dvd.mountedImage = false;

    if (file == nullptr)
    {
        return true;
    }

    // open GCM file
    _tfopen_s(&gcm_file, file, _T("rb"));
    if(!gcm_file) return false;

    // get file size
    fseek(gcm_file, 0, SEEK_END);
    gcm_size = ftell(gcm_file);
    fseek(gcm_file, 0, SEEK_SET);

    fclose(gcm_file);

    // protect from damaged GCMs
    if(gcm_size < DVD_APPLDR_OFFSET)
    {
        return false;
    }

    // reset position
    seekval = 0;

    _tcscpy_s(gcm_filename, _countof(gcm_filename) - 1, file);
    dvd.mountedImage = true;

    return true;
}

void GCMSeek(int position)
{
    seekval = position;
}

void GCMRead(uint8_t*buf, size_t length)
{
    FILE* gcm_file;

    if (gcm_filename[0] == 0)
    {
        memset(buf, 0, length);        // fill by zeroes
        return;
    }

    _tfopen_s (&gcm_file, gcm_filename, _T("rb"));

    if(gcm_file)
    {
        // out of DVD
        if(seekval >= DVD_SIZE)
        {
            memset(buf, 0, length);     // fill by zeroes
            seekval += (int)length;
            fclose(gcm_file);
            return;
        }

        // GCM files can be less than 1.4 GB,
        // so just return zeroes, when seek is out of file
        if(seekval >= gcm_size)
        {
            memset(buf, 0, length);     // fill by zeroes
            seekval += (int)length;
            fclose(gcm_file);
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
            // https://stackoverflow.com/questions/295994/what-is-the-rationale-for-fread-fwrite-taking-size-and-count-as-arguments
            fread(buf, 1, length, gcm_file);
            fclose(gcm_file);
            seekval += (int)length;
        }
    }
    else memset(buf, 0, length);        // fill by zeroes
}
