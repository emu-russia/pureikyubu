#pragma once

#pragma pack(push, 1)

struct RGB
{
    uint8_t Blue;
    uint8_t Green;
    uint8_t Red;
    uint8_t Reserved;
};

#pragma pack(pop)

bool VideoOutOpen(HWConfig *config, int width, int height, RGB **gfxbuf);
void VideoOutClose();
void VideoOutRefresh();
void VideoOutResize(int width, int height);
