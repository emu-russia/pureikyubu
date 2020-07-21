// TODO: The proposal to throw out this functionality, because it is mixed with the HW components.
// Modern development environments (Visual Studio) contain more advanced profiling tools.

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
