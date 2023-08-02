// very simple GCM reading (for .gcm files)

// externals for DVD callbacks (see DVD.h)
bool    GCMMountFile(const wchar_t *file);
void    GCMSeek(int position);
bool    GCMRead(uint8_t *buf, size_t length);
