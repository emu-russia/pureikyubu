// externals for DVD callbacks (see DVD.h)
BOOL    GCMSelectFile(char *file);
void    GCMSeek(long position);
void    GCMRead(u8 *buf, long length);
void    GCMClose();
