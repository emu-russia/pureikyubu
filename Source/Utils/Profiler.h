// controls
VOID OpenProfiler (VOID);
VOID UpdateProfiler (VOID);

// additive profiler calls
VOID BeginProfileGfx (VOID);  // for gfx
VOID EndProfileGfx (VOID);
VOID BeginProfileSfx (VOID);  // for sfx
VOID EndProfileSfx (VOID);
VOID BeginProfilePAD (VOID);  // for pad
VOID EndProfilePAD (VOID);
VOID BeginProfileDVD (VOID);  // for dvd
VOID EndProfileDVD (VOID);
