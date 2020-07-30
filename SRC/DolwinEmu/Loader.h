
#pragma once

uint32_t LoadDOL(const std::wstring& dolname);

uint32_t LoadELF(const std::wstring& elfname);

uint32_t LoadBIN(const std::wstring& binname);

/* Loader API */
void LoadFile(const std::wstring& filename);
