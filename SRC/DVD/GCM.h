// externals for DVD callbacks (see DVD.h)
bool    GCMMountFile(const TCHAR *file);
void    GCMSeek(int position);
bool    GCMRead(uint8_t *buf, size_t length);
