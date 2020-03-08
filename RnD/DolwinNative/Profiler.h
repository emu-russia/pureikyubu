// controls
void    OpenProfiler(bool enabled);
void    UpdateProfiler();

// additive profiler calls
void    BeginProfileGfx();  // for gfx
void    EndProfileGfx();
void    BeginProfileSfx();  // for sfx
void    EndProfileSfx();
void    BeginProfilePAD();  // for pad
void    EndProfilePAD();
void    BeginProfileDVD();  // for dvd
void    EndProfileDVD();
