// externals for DVD callbacks (see DVD.h)
BOOL    GCMSelectFile(char *file);
void    GCMSeek(int position);
void    GCMRead(uint8_t *buf, int length);
void    GCMClose();
