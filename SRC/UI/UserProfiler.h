// controls
void    OpenProfiler(bool enable);
void    UpdateProfiler();

// additive profiler calls
void    BeginProfileGfx();  // for gfx
void    EndProfileGfx();
void    BeginProfileSfx();  // for sfx
void    EndProfileSfx();
void    BeginProfileDVD();  // for dvd
void    EndProfileDVD();
