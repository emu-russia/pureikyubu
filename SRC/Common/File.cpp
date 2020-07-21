#include "pch.h"

namespace Util
{
    /* Load data from a unicode file. */
    std::vector<uint8_t> FileLoad(std::wstring & filename)
    {
        auto file = std::ifstream(filename, std::ifstream::binary);
        if (!file.is_open())
        {
            return std::vector<uint8_t>();
        }
        else
        {
            auto itr = std::istreambuf_iterator<char>(file);
            return std::vector<uint8_t>(itr, std::istreambuf_iterator<char>());
        }
    }

    /* Load data from a file. */
    std::vector<uint8_t> FileLoad(std::string & filename)
    {
        auto file = std::ifstream(filename, std::ifstream::binary);
        if (!file.is_open())
        {
            return std::vector<uint8_t>();
        }
        else
        {
            return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>());
        }
    }
}
