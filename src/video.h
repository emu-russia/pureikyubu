#pragma once

#pragma pack(push, 1)
union RGB
{
	struct {
		uint8_t Blue;
		uint8_t Green;
		uint8_t Red;
		uint8_t Reserved;
	};
	uint32_t raw;
};
#pragma pack(pop)

bool VideoOutOpen(HWConfig* config, int width, int height, RGB** gfxbuf);
void VideoOutClose();
void VideoOutRefresh();
void VideoOutResize(int width, int height);

//! How many times the back end has output the XFB since the previous call: the pictures the video
//! output window has shown. It is the frame rate a front end reports while the GL backend is not
//! the one presenting the emulated picture - the software GFX pipeline, or a machine whose GL
//! backend never started - and the answer is 0 while the GL backend owns the window, because then
//! the frames are counted where they are presented (see the PE_COPY_CMD handling and the window
//! titles in uisdl.cpp). The null back end of the headless build outputs nothing and always
//! answers 0.
int64_t VideoOutTakeFrames();

