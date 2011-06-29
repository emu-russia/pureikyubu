#define GMP_ID  "GCM_CMPR"  // ID for Dolwin compressed GCM file

// externals for DVD callbacks (see DVD.h)
BOOL    GCMPSelectFile(char *file);
void    GCMPSeek(long position);
void    GCMPRead(u8 *buf, long length);
void    GCMPClose();
