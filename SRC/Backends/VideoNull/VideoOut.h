#pragma once

bool VideoOutOpen(HWConfig *config, int width, int height, RGB **gfxbuf);
void VideoOutClose();
void VideoOutRefresh();
void VideoOutResize(int width, int height);
