
#pragma once

uint32_t LoadDOL(std::wstring& dolname);

uint32_t LoadELF(std::wstring& elfname);

uint32_t LoadBIN(std::wstring& binname);

/* Loader API */
void LoadFile(std::wstring& filename);
