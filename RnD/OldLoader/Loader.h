
#pragma once

uint32_t DOLSize(DolHeader *dol);
uint32_t LoadDOL(std::wstring& dolname);
uint32_t LoadDOLFromMemory(DolHeader *dol, uint32_t ofs);

uint32_t LoadELF(std::wstring& elfname);

/* load binary file */

uint32_t LoadBIN(std::wstring& binname);

/* Loader API */
void LoadFile(std::wstring& filename);
void LoadFile(std::string& filename);

/* All loader variables are placed here */
struct LoaderData
{
    uint32_t            binOffset;          // binary file loading offset
    std::wstring        cwd;                // current working directory
    std::wstring        gameID;             // GameID, for dvd
    std::wstring        currentFile;        // next file to be loaded or re-loaded
    std::wstring        currentFileName;    // name of loaded file (without extension)
    float               boottime;           // in seconds
    bool                dvd;                // true: loaded file is DVD image
};

extern LoaderData ldat;
