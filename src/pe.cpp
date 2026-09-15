// PE - pixel engine
#include "pch.h"

using namespace Debug;

// Handling access to the PE registers available to the CPU and EFB

namespace GFX
{
	void PixelEngine::PE_DONE_INT()
	{
		if (peregs.sr & PE_SR_DONEMSK)
		{
			peregs.sr |= PE_SR_DONE;
			Flipper::HW->pi->PIAssertInt(PI_INTERRUPT_PE_FINISH);
		}
	}

	void PixelEngine::PE_TOKEN_INT()
	{
		if (peregs.sr & PE_SR_TOKENMSK)
		{
			peregs.sr |= PE_SR_TOKEN;
			Flipper::HW->pi->PIAssertInt(PI_INTERRUPT_PE_TOKEN);
		}
	}

	// Currently, Cpu2Efb emulation is not supported. It is planned to be forwarded to the graphic Backend.

	uint32_t PixelEngine::EfbPeek(uint32_t addr)
	{
		Report(Channel::GP, "EfbPeek, address: 0x%08X\n", addr);
		return 0;
	}

	void PixelEngine::EfbPoke(uint32_t addr, uint32_t value)
	{
		Report(Channel::GP, "EfbPoke, address: 0x%08X, value: 0x%08X\n", addr, value);
	}

	// sel:0 - file, sel:1 - memory
	void PixelEngine::GL_DoSnapshot(bool sel, FILE* f, uint8_t* dst, int width, int height)
	{
		uint8_t      hdr[14 + 40];   // bmp header
		uint16_t* phdr;
		uint16_t     s, t;
		uint8_t* buf, * ptr;
		float   ds, dt, d0, d1;
		bool    linear = false;

		// allocate temporary buffer
		buf = (uint8_t*)malloc(gfx->scr_w * gfx->scr_h * 3);

		// calculate aspects
		ds = (float)gfx->scr_w / (float)width;
		dt = (float)gfx->scr_h / (float)height;
		if (ds != 1.0f) linear = true;

		// write hardcoded header
		memset(hdr, 0, sizeof(hdr));
		hdr[0] = 'B'; hdr[1] = 'M'; hdr[2] = 0x36;
		hdr[4] = 0x20; hdr[10] = 0x36;
		hdr[14] = 40;
		phdr = (uint16_t*)(&hdr[0x12]); *phdr = (uint16_t)width;
		phdr = (uint16_t*)(&hdr[0x16]); *phdr = (uint16_t)height;
		hdr[26] = 1; hdr[28] = 24; hdr[36] = 0x20;
		if (sel)
		{
			memcpy(dst, hdr, sizeof(hdr));
			dst += sizeof(hdr);
		}
		else fwrite(hdr, 1, sizeof(hdr), f);

		// read opengl buffer
		glReadPixels(0, 0, gfx->scr_w, gfx->scr_h, GL_RGB, GL_UNSIGNED_BYTE, buf);

		// write texture image
		for (t = 0, d0 = 0; t < height; t++, d0 += dt)
		{
			for (s = 0, d1 = 0; s < width; s++, d1 += ds)
			{
				uint8_t  prev[3] = { 0 };
				uint8_t  rgb[3];     // RGB triplet
				ptr = &buf[3 * (gfx->scr_w * (int)d0 + (int)d1)];
				{
					// linear filter
					if (s && linear)
					{
						rgb[2] = (*ptr++ + prev[2]) >> 1;
						rgb[1] = (*ptr++ + prev[1]) >> 1;
						rgb[0] = (*ptr++ + prev[0]) >> 1;
					}
					else
					{
						rgb[2] = *ptr++;
						rgb[1] = *ptr++;
						rgb[0] = *ptr++;
					}

					if (linear)
					{
						prev[2] = rgb[2];
						prev[1] = rgb[1];
						prev[0] = rgb[0];
					}

					if (sel) { memcpy(dst, rgb, 3); dst += 3; }
					else fwrite(rgb, 1, 3, f);
				}
			}
		}

		free(buf);
	}

	// make small snapshot for savestate
	// new size 160x120
	void PixelEngine::GL_SaveBitmap(uint8_t* buf)
	{
		GL_DoSnapshot(true, NULL, buf, 160, 120);
	}

	void PixelEngine::PERegRead(uint32_t addr, uint32_t* reg, void* context)
	{
		PixelEngine* pe = (PixelEngine*)context;
		switch (addr & 0xFF)
		{
			case PE_PI_INTRCTRL:
				*reg = pe->peregs.sr;
				break;

			case PE_PI_TOKEN:
				*reg = pe->pe.token.token;
				break;

			default:
				*reg = 0;
				break;
		}
	}

	void PixelEngine::PERegWrite(uint32_t addr, uint32_t data, void* context)
	{
		PixelEngine* pe = (PixelEngine*)context;
		switch (addr & 0xFF)
		{
			case PE_PI_INTRCTRL:

				// clear interrupts
				if (pe->peregs.sr & PE_SR_DONE)
				{
					pe->peregs.sr &= ~PE_SR_DONE;
					Flipper::HW->pi->PIClearInt(PI_INTERRUPT_PE_FINISH);
				}
				if (pe->peregs.sr & PE_SR_TOKEN)
				{
					pe->peregs.sr &= ~PE_SR_TOKEN;
					Flipper::HW->pi->PIClearInt(PI_INTERRUPT_PE_TOKEN);
				}

				// set mask bits
				if (data & PE_SR_DONEMSK) pe->peregs.sr |= PE_SR_DONEMSK;
				else pe->peregs.sr &= ~PE_SR_DONEMSK;
				if (data & PE_SR_TOKENMSK) pe->peregs.sr |= PE_SR_TOKENMSK;
				else pe->peregs.sr &= ~PE_SR_TOKENMSK;

				break;

			default:
				break;
		}
	}

	PixelEngine::PixelEngine(Flipper::Flipper* flipper, HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;

		Report(Channel::CP, "Pixel Engine (for GFX)\n");

		// Pixel Engine
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_ZMODE, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CMODE0, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CMODE1, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_ALPHA_THRES, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CONTROL, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_INTRCTRL, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_TOKEN, PERegRead, PERegWrite, this);

		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_XBOUND0, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_XBOUND1, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_YBOUND0, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_YBOUND1, PERegRead, PERegWrite, this);

		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_0L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_0H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_1L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_1H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_2L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_2H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_3L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_3H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_4L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_4H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_5L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_5H, PERegRead, PERegWrite, this);
	}

	PixelEngine::~PixelEngine()
	{
	}

	void PixelEngine::loadPEReg(size_t index, uint32_t value)
	{
		switch (index)
		{
			// Pixel Engine block

			case PE_ZMODE_ID:
				pe.zmode.bits = value;
				ApplyZMode();
				break;

			case PE_CMODE0_ID:
				pe.cmode0.bits = value;
				ApplyColorMode();
				break;

			case PE_CMODE1_ID:
				pe.cmode1.bits = value;
				ApplyColorMode();
				break;

			// The EFB pixel type / Z compression format. Neither has an equivalent in the OpenGL
			// backend: the render target is always the RGBA8 colour buffer with a 24-bit depth buffer,
			// which is what the uncompressed ("none") pixel types describe.
			case PE_CONTROL_ID:
				pe.control.bits = value;
				break;

			case PE_FIELD_MASK_ID:
				pe.field_mask.bits = value;
				break;

			// draw done
			case PE_FINISH_ID:
			{
				// GXDrawDone marks the end of the frame: the copied picture is complete.
				gfx->GPFrameDone();

				pe_done_num++;
				PE_DONE_INT();
			}
			break;

			// token
			case PE_TOKEN_INT_ID:
				pe.token_int.bits = value;
				break;

			// draw sync token
			//
			// The token is a synchronisation marker, not a frame boundary: a title loads it through
			// the pipe to learn when the PE has walked the FIFO up to that point, and several demos
			// put it in the middle of a frame (draw, sync, read the bounding box back, draw some
			// more). Presenting the frame here swapped half a frame out and left the other buffer -
			// which held only what was drawn after the token - as the finished picture, so
			// frb-bound-box came out black. The picture is complete at GXDrawDone (PE_FINISH above)
			// and at a full-frame display copy (GPDisplayCopy), which is where the frame is
			// presented; the token only raises its interrupt.
			case PE_TOKEN_ID:
			{
				pe.token.bits = value;
				if (pe.token.token == pe.token_int.token)
				{
					PE_TOKEN_INT();
				}
			}
			break;

			case PE_REFRESH_ID:
				pe.refresh.bits = value;
				break;

			case PE_COPY_SRC_ADDR_ID:
				pe.copy_src_addr.bits = value;
				break;

			case PE_COPY_SRC_SIZE_ID:
				pe.copy_src_size.bits = value;
				break;

			case PE_COPY_DST_BASE0_ID:
				pe.copy_dst_base[0].bits = value;
				break;

			case PE_COPY_DST_BASE1_ID:
				pe.copy_dst_base[1].bits = value;
				break;

			case PE_COPY_DST_STRIDE_ID:
				pe.copy_dst_stride.bits = value;
				break;

			case PE_COPY_SCALE_ID:
				pe.copy_scale.bits = value;
				break;

			case PE_COPY_CLEAR_AR_ID:
				pe.copy_clear_ar.bits = value;
				break;

			case PE_COPY_CLEAR_GB_ID:
				pe.copy_clear_gb.bits = value;
				break;

			case PE_COPY_CLEAR_Z_ID:
				pe.copy_clear_z.bits = value;
				break;

			// The copy command is the trigger of the whole copy engine.
			//
			// A copy and its clear belong together, in the order the stream asks for them: the copy
			// reads the rectangle as it is, and a copy that asked for a clear turns the quads it
			// read into the clear colour (gfx-pe.md 5.1, the RMW of the colour unit), so whatever
			// follows sees the cleared EFB. See below for when the clear of each kind runs.
			//
			// The values are captured when the command arrives, because the game may reprogram the
			// registers for its next copy before the clear runs.
			case PE_COPY_CMD_ID:
			{
				pe.copy_cmd.bits = value;

				CopyClearState clear{};

				if (pe.copy_cmd.clear)
				{
					clear.ar = pe.copy_clear_ar;
					clear.gb = pe.copy_clear_gb;
					clear.z = pe.copy_clear_z;
					clear.x = (int)pe.copy_src_addr.x;
					clear.y = (int)pe.copy_src_addr.y;
					clear.w = (int)pe.copy_src_size.x + 1;
					clear.h = (int)pe.copy_src_size.y + 1;

					// The two kinds of copy clear differently. A texture copy prepares a render
					// target the title is about to draw into: its clear runs below, over the
					// rectangle it read, before those draws - a title that renders its frame in
					// several passes through the copy engine loses the passes drawn before it if
					// that clear wipes the whole buffer. A display copy presents the frame, and this
					// backend shows the EFB where a console shows the XFB, so its clear covers the
					// whole colour buffer and waits for the frame begin: running it here would wipe
					// the picture that is about to be shown, and leaving any part of the buffer
					// alone kept the previous frame in the lower half of the bootrom's splash.
					if (pe.copy_cmd.opcode == PE_COPY_CMD_DISPLAY)
					{
						clear.full = true;

						if (pending_clear_count < MaxPendingCopyClears)
						{
							pending_clears[pending_clear_count++] = clear;
						}
					}
				}

				// A display copy hands the finished EFB over to the video interface as the XFB
				// (gfx-pe.md 5.6), so the full-frame ones are a frame boundary of their own. The
				// backend displays the EFB instead of the XFB, so the picture has to be swapped
				// here for the titles whose movie player presents through the copy engine and waits
				// for the retrace without ever calling GXDrawDone (the SDK THP player does that; its
				// frames stayed on an unpresented back buffer, issue #349).
				//
				// A partial display copy is not a frame boundary: the bootrom and the 2D front ends
				// write the picture in several passes (one copy per display-list buffer) and call
				// PE_FINISH when the frame is complete. Presenting those would flicker the picture.
				//
				// A texture copy is an intermediate render target and never presents: it turns the
				// rectangle into a tiled texture in main memory (gfx-pe.md 5.7).
				if (pe.copy_cmd.opcode == PE_COPY_CMD_TEXTURE)
				{
					TextureCopy();
				}
				else if (gfx->SoftPipeline())
				{
					// The display copy of the software pipeline is what the console really does: it
					// converts the EFB rectangle into the packed YUV 4:2:2 XFB in main memory, which
					// the video interface then scans out (gfx-pe.md 5.6). The GL backend has no XFB
					// and shows the EFB instead, so it skips this.
					SoftDisplayCopy();
				}

				if (pe.copy_cmd.opcode == PE_COPY_CMD_DISPLAY &&
					pe.copy_src_addr.x == 0 && pe.copy_src_size.x + 1 >= gfx->RenderWidth())
				{
					gfx->GPDisplayCopy();
				}

				// A texture copy's clear belongs to the copy itself and runs right here: it only
				// prepares the EFB for the copies that follow, and it cannot disturb the frame a
				// display copy presents. The clear of a *display* copy stays deferred to the frame
				// begin: the backend shows the EFB in place of the XFB the hardware would have
				// written first, so clearing it here would wipe the picture that is about to be
				// shown (the bootrom screen went black that way).
				if (pe.copy_cmd.clear && pe.copy_cmd.opcode == PE_COPY_CMD_TEXTURE)
				{
					ApplyCopyClear(clear);
				}
				break;
			}

			case PE_COPY_VFILTER0_ID:
				pe.vfilter_0.bits = value;
				break;

			case PE_COPY_VFILTER1_ID:
				pe.vfilter_1.bits = value;
				break;

			case PE_XBOUND_ID:
				pe.xbound.bits = value;
				break;

			case PE_YBOUND_ID:
				pe.ybound.bits = value;
				break;

			case PE_PERFMODE_ID:
				pe.perfmode.bits = value;
				break;

			case PE_CHICKEN_ID:
				pe.chicken.bits = value;
				break;

			case PE_QUAD_OFFSET_ID:
				pe.quad_offset.bits = value;
				break;

			default:
				// The BP register walk continues in the bump/indirect unit, then the texture unit and
				// the TEV (see gfx.md 10.1)
				gfx->bump->loadBUMPReg(index, value);
				break;
		}
	}

	// -------------------------------------------------------------------------------------------
	// The GL state the PE owns
	// -------------------------------------------------------------------------------------------

	// PE_ZMODE: enable, compare function and Z write mask. GEN_MODE.zfreeze freezes the Z buffer
	// after the first primitive, which in this backend means that no depth is written any more
	// (gfx-su.md 3.6, gfx-pe.md 4.2).
	void PixelEngine::ApplyZMode()
	{
		// The software pipeline owns no GL state at all: what the registers describe is applied by
		// the software Z unit while it writes a pixel (see SoftWritePixel).
		if (gfx != nullptr && gfx->SoftPipeline())
			return;

		static const GLenum glzf[8] = {
			GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
		};

		if (pe.zmode.enable)
		{
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(glzf[pe.zmode.func & 7]);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		bool zfreeze = (gfx != nullptr) && (gfx->genmode.zfreeze != 0);
		glDepthMask((pe.zmode.mask && !zfreeze) ? GL_TRUE : GL_FALSE);
	}

	void PixelEngine::ApplyColorMode()
	{
		// See ApplyZMode: the software pipeline applies the colour mode per pixel.
		if (gfx != nullptr && gfx->SoftPipeline())
			return;

		// Blend factors (gfx-pe.md 6.2). The first four differ between the source and the
		// destination factor; the last four are shared.
		static const GLenum glsf[8] = {
			GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA
		};

		static const GLenum gldf[8] = {
			GL_ZERO, GL_ONE, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA
		};

		if (pe.cmode0.blend_en)
		{
			glEnable(GL_BLEND);

			GLenum src = glsf[pe.cmode0.sfactor & 7];
			GLenum dst = gldf[pe.cmode0.dfactor & 7];

			// PE_CMODE1.const_alpha replaces the destination alpha factor with a constant
			if (pe.cmode1.const_alpha_en)
			{
				glBlendColor(1.0f, 1.0f, 1.0f, (float)pe.cmode1.const_alpha / 255.0f);

				if (dst == GL_DST_ALPHA) dst = GL_CONSTANT_ALPHA;
				else if (dst == GL_ONE_MINUS_DST_ALPHA) dst = GL_ONE_MINUS_CONSTANT_ALPHA;
			}

			glBlendFunc(src, dst);

			// PE_CMODE0.blendop: 0 = add, 1 = subtract (destination minus source)
			glBlendEquation(pe.cmode0.blendop ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
		}
		else
		{
			glDisable(GL_BLEND);
		}

		static const GLenum logop[16] = {
			GL_CLEAR, GL_AND, GL_AND_REVERSE, GL_COPY, GL_AND_INVERTED, GL_NOOP, GL_XOR, GL_OR,
			GL_NOR, GL_EQUIV, GL_INVERT, GL_OR_REVERSE, GL_COPY_INVERTED, GL_OR_INVERTED, GL_NAND, GL_SET
		};

		if (pe.cmode0.logop_en)
		{
			glEnable(GL_COLOR_LOGIC_OP);
			glLogicOp(logop[pe.cmode0.logop & 0xf]);
		}
		else
		{
			glDisable(GL_COLOR_LOGIC_OP);
		}

		// The two mask bits are "update enabled" flags, not "do not update": GXSetColorUpdate and
		// GXSetAlphaUpdate store their argument straight into them, so a set bit writes the plane.
		glColorMask(
			pe.cmode0.col_mask ? GL_TRUE : GL_FALSE,
			pe.cmode0.col_mask ? GL_TRUE : GL_FALSE,
			pe.cmode0.col_mask ? GL_TRUE : GL_FALSE,
			pe.cmode0.alpha_mask ? GL_TRUE : GL_FALSE);

		// Dither (PE_CMODE0.dither_en, gfx-pe.md 6.3). GL_DITHER is set from the register so that the
		// state is not silently dropped, but it cannot reproduce the hardware dither in this backend:
		// the Flipper dithers with a 4x4 ordered matrix when the EFB pixel format stores fewer bits
		// per channel than the TEV produces (RGB565 or RGBA6), while the render target of this
		// backend is 8 bits per channel for every format. For the RGB8 EFB format - the only one the
		// backend implements - the dither matrix is the identity, and for the narrower ones the
		// backend has no render target to quantise into. A faithful implementation has to start with
		// the EFB pixel format (PE_ZCONF / PE_CONTROL.pixel_format), which the emulator does not
		// decode yet. See wiki/gfx.md.
		if (pe.cmode0.dither_en)
			glEnable(GL_DITHER);
		else
			glDisable(GL_DITHER);
	}

	//! Read a rectangle of the EFB into an RGB buffer, top row first. This is the only way back from
	//! the copy engine's round trip through the colour buffer, which is what makes it a pixel engine
	//! operation (gfx-pe.md 5). The software pipeline reads its own CPU-side EFB, the shader
	//! pipeline the GL render target; both live in the single implementation at the end of this
	//! file, next to the software pixel engine.

	// The copy engine's clear fills the EFB with the PE clear colour and the clear Z, without depth
	// testing or blending, using the values the copy that asked for it was programmed with (see
	// CopyClearState). The hardware clears only the rectangle the copy read, but the backend displays
	// the whole EFB (a real console shows the scaled XFB instead), so the whole render target is
	// cleared here: leaving the rest of it alone smeared the previous frame into the part of the
	// picture the copy does not cover.
	// The copy engine turns a rectangle of the EFB into a tiled texture in main memory. The tiling
	// is the one the texture unit reads back (see tx.cpp) applied in reverse: the tiles of a
	// texture are stored row by row, and the shape of a tile follows the texel size of the
	// destination format. The tile rows are `PE_COPY_DST_STRIDE` cache lines apart, so they are
	// not necessarily contiguous (gfx-pe.md 5.7).
	void PixelEngine::TextureCopy()
	{
		int w = (int)pe.copy_src_size.x + 1;
		int h = (int)pe.copy_src_size.y + 1;
		int srcX = (int)pe.copy_src_addr.x;
		int srcY = (int)pe.copy_src_addr.y;
		int fmt = (int)pe.copy_cmd.tex_format | ((int)pe.copy_cmd.tex_format_h << 3);
		uint32_t dst = (uint32_t)pe.copy_dst_base[0].base << 5;

		if (w <= 0 || h <= 0)
			return;

		// A rectangle that runs off the EFB is clamped: the tiles it covers are written anyway.
		if (srcX < 0) { w += srcX; srcX = 0; }
		if (srcY < 0) { h += srcY; srcY = 0; }
		if (srcX + w > (int)gfx->RenderWidth()) w = (int)gfx->RenderWidth() - srcX;
		if (srcY + h > (int)gfx->RenderHeight()) h = (int)gfx->RenderHeight() - srcY;

		if (w <= 0 || h <= 0)
			return;

		// The shape of a tile follows the texel size of the format (gfx-pe.md 5.7).
		int tileW = 4, tileH = 4;

		switch (fmt)
		{
			case TF_I4:
			case TF_C4:
				tileW = 8; tileH = 8;
				break;

			case TF_I8:
			case TF_IA4:
			case TF_C8:
				tileW = 8; tileH = 4;
				break;

			case TF_IA8:
			case TF_RGB565:
			case TF_RGB5A3:
			case TF_C14:
			case TF_RGBA8:
				tileW = 4; tileH = 4;
				break;

			default:
				return;
		}

		// The copy engine reads whole tiles: the destination holds the tiles the rectangle covers,
		// so a rectangle that is not a multiple of the tile size is padded with the edge texels.
		int tilesX = (w + tileW - 1) / tileW;
		int tilesY = (h + tileH - 1) / tileH;

		// The copy engine reads its source out of the EFB, which is this unit's own colour buffer.
		// The rectangle is in screen coordinates (the origin is the top left corner): the shader
		// backend reads it through glReadPixels, which counts from the bottom, while the software
		// EFB is addressed top down like the window.
		int readY = gfx->SoftPipeline() ? srcY : ((int)gfx->RenderHeight() - srcY - h);

		std::vector<uint8_t> rgb;
		if (!ReadEfb(srcX, readY, w, h, rgb))
			return;

		auto texel = [&](int x, int y, int c) -> uint8_t
		{
			if (x >= w) x = w - 1;
			if (y >= h) y = h - 1;
			if (x < 0) x = 0;
			if (y < 0) y = 0;
			return rgb[((size_t)y * w + x) * 3 + c];
		};

		// The intensity formats take the luma of the EFB colour: the RGB to Y conversion of the
		// copy path is applied automatically for a copy into an intensity format (gfx-pe.md 5.4).
		auto luma = [&](int x, int y) -> uint8_t
		{
			int r = texel(x, y, 0), g = texel(x, y, 1), b = texel(x, y, 2);
			int y_ = (66 * r + 129 * g + 25 * b + 128) >> 8;
			return (uint8_t)(y_ < 0 ? 0 : (y_ > 255 ? 255 : y_));
		};

		uint8_t* dstPtr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForPI(dst);
		if (dstPtr == nullptr)
			return;

		// The stride is the distance between the first texel of a tile and the first texel of the
		// tile below it, in cache lines.
		size_t stride = (size_t)pe.copy_dst_stride.stride * 32;

		for (int ty = 0; ty < tilesY; ty++)
		{
			// A tile of a 32-bit format spans two cache lines, every other one is a single line.
			uint8_t* p = dstPtr + (size_t)ty * stride;

			for (int tx = 0; tx < tilesX; tx++)
			{
				int bx = tx * tileW;
				int by = ty * tileH;

				switch (fmt)
				{
					case TF_I4:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u += 2)
							{
								uint8_t hi = luma(bx + u, by + v) >> 4;
								uint8_t lo = luma(bx + u + 1, by + v) >> 4;
								*p++ = (uint8_t)((hi << 4) | lo);
							}
						break;
					}

					case TF_I8:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
								*p++ = luma(bx + u, by + v);
						break;
					}

					case TF_IA4:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
								*p++ = (uint8_t)((texel(bx + u, by + v, 3) & 0xf0) | (luma(bx + u, by + v) >> 4));
						break;
					}

					case TF_IA8:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
							{
								*p++ = texel(bx + u, by + v, 3);
								*p++ = luma(bx + u, by + v);
							}
						break;
					}

					case TF_RGB565:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
							{
								uint16_t c = (uint16_t)(((texel(bx + u, by + v, 0) >> 3) << 11) |
									((texel(bx + u, by + v, 1) >> 2) << 5) |
									(texel(bx + u, by + v, 2) >> 3));
								*p++ = (uint8_t)(c >> 8);
								*p++ = (uint8_t)c;
							}
						break;
					}

					case TF_RGB5A3:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
							{
								int r = texel(bx + u, by + v, 0), g = texel(bx + u, by + v, 1), b = texel(bx + u, by + v, 2);
								int a = texel(bx + u, by + v, 3);
								uint16_t c;
								if (a > 0xe0)
								{
									c = (uint16_t)(0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
								}
								else
								{
									c = (uint16_t)(((a >> 5) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
								}
								*p++ = (uint8_t)(c >> 8);
								*p++ = (uint8_t)c;
							}
						break;
					}

					case TF_RGBA8:
					{
						// A 4x4 block of this format spans two cache lines: alpha/red pairs first,
						// then the green/blue ones.
						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
							{
								*p++ = texel(bx + u, by + v, 3);
								*p++ = texel(bx + u, by + v, 0);
							}
						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
							{
								*p++ = texel(bx + u, by + v, 1);
								*p++ = texel(bx + u, by + v, 2);
							}
						break;
					}

					default:
						return;
				}
			}
		}
	}

	// The colour word of the EFB lane (defined with the software pixel engine below)
	static uint32_t SoftPackEfbColor(int r, int g, int b, int a);

	void PixelEngine::ApplyCopyClear(const CopyClearState& clear)
	{
		// A texture copy's clear covers the rectangle the copy reads (gfx-pe.md 5.1): the copy engine
		// turns every quad it reads into the clear colour and leaves the rest of the EFB as it was.
		// A display copy's clear covers the whole buffer instead, because this backend shows the EFB
		// where a console scans out the XFB the copy wrote (see CopyClearState::full).
		int x = clear.x, y = clear.y, w = clear.w, h = clear.h;

		if (clear.full || w <= 0 || h <= 0)
		{
			x = 0; y = 0; w = (int)gfx->scr_w; h = (int)gfx->scr_h;
		}

		// The software pipeline clears its own EFB memory: the clear word is the EFB lane the
		// colour unit stores (blue in the low byte, then green, red and alpha).
		if (gfx->SoftPipeline())
		{
			uint32_t rgba = SoftPackEfbColor(clear.ar.red, clear.gb.green, clear.gb.blue, clear.ar.alpha);
			SoftClearRect(x, y, w, h, rgba, clear.z.value);
			return;
		}

		glScissor(x, (int)gfx->scr_h - (y + h), w, h);
		glDisable(GL_BLEND);
		glDisable(GL_COLOR_LOGIC_OP);
		glDisable(GL_DEPTH_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		// The copy engine writes the whole region, so the Z update mask does not apply either: a GL
		// clear of the depth buffer would otherwise be skipped when the scene left the mask disabled.
		// ApplyZMode() below puts the mask back.
		glDepthMask(GL_TRUE);

		glClearColor(
			(float)clear.ar.red / 255.0f,
			(float)clear.gb.green / 255.0f,
			(float)clear.gb.blue / 255.0f,
			(float)clear.ar.alpha / 255.0f);
		glClearDepth((double)(clear.z.value / 16777215.0));
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// The clear bypassed the GL state the registers describe, so it is re-applied
		ApplyZMode();
		ApplyColorMode();

		glScissor(0, 0, (GLsizei)gfx->scr_w, (GLsizei)gfx->scr_h);
	}

	bool PixelEngine::ApplyPendingCopyClears()
	{
		if (pending_clear_count == 0)
		{
			return false;
		}

		for (size_t i = 0; i < pending_clear_count; i++)
		{
			ApplyCopyClear(pending_clears[i]);
		}

		pending_clear_count = 0;
		return true;
	}

	void PixelEngine::Reset()
	{
		pe = PEState{};
		peregs = PERegs{};
		pending_clear_count = 0;

		// The hardware reset values that the specification states (gfx-pe.md 6.5, 6.8, 6.20)
		pe.field_mask.bits = 0x3;
		// The colour and alpha update bits are set as well: a program that never programs
		// PE_CMODE0 (the GX SDK does, in GXInit) would otherwise be unable to draw at all.
		pe.cmode0.col_mask = 1;
		pe.cmode0.alpha_mask = 1;
		pe.quad_offset.bits = 0xAAAAAAAA;
		pe.token_int.bits = 0xFFFF;

		frames = 0;
		pe_done_num = 0;

		if (gfx != nullptr && gfx->BackendStarted())
		{
			gfx->ApplyDefaultGLState();
			ApplyZMode();
			ApplyColorMode();
		}

		// The software EFB starts empty; the copy engine's clear fills it (SoftBeginFrame).
		SoftAlloc();
	}

	// -------------------------------------------------------------------------------------------
	// The software Pixel Engine (GFX_PIPELINE = soft, issue #384)
	//
	// The software EFB is a real memory array, like the eDRAM of the hardware: the window address
	// of a pixel is decoded the way gfx-pe.md 3.3 describes it - the pixel word sits at
	// `y * 1024 + x` (the 1K-pixel stride that makes X and Y extract straight from the address
	// bits) and the address bit 22 selects the Z plane instead of the colour plane.
	//
	// The word of a colour pixel is the CPU view of the eDRAM lane: `{blue, green, red, alpha}`
	// from the least significant byte up (`pe_color_rgb8` of gfx-pe.md 8.1 plus the alpha byte of
	// the alpha-bearing formats), and the Z word carries the 24-bit depth.
	//
	// The datapath below is the RMW pipeline of gfx-pe.md 4.1: the Z test of the Z unit (4.2) and
	// the blend / logic op with the write masks of the colour unit (4.3). The frame drawn here is
	// not presented by GL: the copy engine's display copy writes it into the XFB in main memory
	// and the video interface scans that out, exactly like a real console.
	// -------------------------------------------------------------------------------------------

	//! The pixel stride of the EFB window: the hardware uses a 1K-pixel stride so that the X and
	//! the Y of a pixel extract directly from the address bits (gfx-pe.md 3.3).
	static const int EfbStride = 1024;
	//! Address bit 22 selects the Z plane (gfx-pe.md 3.3): 1M words into the window.
	static const size_t EfbZPlane = 1u << 20;

	//! The colour word of the EFB lane: blue in the low byte, then green, red and alpha.
	static uint32_t SoftPackEfbColor(int r, int g, int b, int a)
	{
		return (uint32_t)(b & 0xFF) | ((uint32_t)(g & 0xFF) << 8) |
			((uint32_t)(r & 0xFF) << 16) | ((uint32_t)(a & 0xFF) << 24);
	}

	static void SoftUnpackEfbColor(uint32_t word, int* r, int* g, int* b, int* a)
	{
		*b = (int)(word & 0xFF);
		*g = (int)((word >> 8) & 0xFF);
		*r = (int)((word >> 16) & 0xFF);
		*a = (int)((word >> 24) & 0xFF);
	}

	void PixelEngine::SoftAlloc()
	{
		int w = (int)(gfx != nullptr ? gfx->RenderWidth() : 640);
		int h = (int)(gfx != nullptr ? gfx->RenderHeight() : 480);

		if (w <= 0) w = 640;
		if (h <= 0) h = 480;

		if (w == soft_w && h == soft_h && !efb.empty())
			return;

		soft_w = w;
		soft_h = h;

		// The colour plane and the Z plane of the window the EFB occupies.
		size_t words = EfbZPlane + (size_t)h * EfbStride;
		efb.assign(words, 0);

		// The Z of a fresh EFB is the far value, so that the first primitive of a frame that never
		// asks for a clear still passes its depth test.
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
				efb[EfbZPlane + (size_t)y * EfbStride + x] = 0xFFFFFF;
		}
	}

	void PixelEngine::SoftClearRect(int x, int y, int w, int h, uint32_t rgba, uint32_t z)
	{
		SoftAlloc();

		if (x < 0) { w += x; x = 0; }
		if (y < 0) { h += y; y = 0; }
		if (x + w > soft_w) w = soft_w - x;
		if (y + h > soft_h) h = soft_h - y;

		if (w <= 0 || h <= 0)
			return;

		for (int row = 0; row < h; row++)
		{
			for (int col = 0; col < w; col++)
			{
				efb[(size_t)(y + row) * EfbStride + x + col] = rgba;
				efb[EfbZPlane + (size_t)(y + row) * EfbStride + x + col] = z & 0xFFFFFF;
			}
		}
	}

	void PixelEngine::SoftBeginFrame()
	{
		SoftAlloc();

		// A display copy asks for the EFB rectangle it read to be cleared: the clear prepares the
		// frame *after* the copy, so it runs here and not when the copy command arrives (the copy
		// has to hand its picture to the XFB first). See CopyClearState. When no copy asked for a
		// clear, the frame is cleared with the PE clear values, exactly like the shader backend's
		// frame begin does (GFXCore::GL_BeginFrame).
		if (ApplyPendingCopyClears())
			return;

		uint32_t rgba = SoftPackEfbColor(pe.copy_clear_ar.red, pe.copy_clear_gb.green,
			pe.copy_clear_gb.blue, pe.copy_clear_ar.alpha);

		SoftClearRect(0, 0, soft_w, soft_h, rgba, pe.copy_clear_z.value);
	}

	//! 8x8 multiply of the blend datapath. The factor is normalized first: its most significant bit
	//! is added to itself, so that 0..127 stay as they are and 128..255 become 129..256, i.e. 255
	//! stands for one rather than for 255/256 (gfx-pe.md 4.3).
	static int SoftBlendMul(int value, int factor)
	{
		factor += (factor >> 7) & 1;
		int r = (value * factor) >> 8;
		return r > 255 ? 255 : r;
	}

	//! One factor of the blend equation (gfx-pe.md 4.3). `source` selects the source factor table;
	//! the two tables differ in the colour terms: source factor 2/3 is the *destination* colour and
	//! destination factor 2/3 is the *source* colour, which is why the GX SDK defines GX_BL_SRCCLR
	//! and GX_BL_DSTCLR with the same value (2).
	static int SoftBlendFactor(int sel, bool source, int comp, const int* src, const int* dst)
	{
		switch (sel & 7)
		{
			case 0: return 0;										// zero
			case 1: return 255;										// one
			case 2: return source ? dst[comp] : src[comp];			// (other) colour
			case 3: return 255 - (source ? dst[comp] : src[comp]);	// one minus that colour
			case 4: return src[3];									// source alpha
			case 5: return 255 - src[3];							// one minus source alpha
			case 6: return dst[3];									// destination alpha
			default: return 255 - dst[3];							// one minus destination alpha
		}
	}

	bool PixelEngine::SoftWritePixel(int x, int y, const float rgba[4], float depth)
	{
		SoftAlloc();

		if (x < 0 || y < 0 || x >= soft_w || y >= soft_h)
			return false;

		size_t idx = (size_t)y * EfbStride + x;

		int src[4];
		for (int c = 0; c < 4; c++)
		{
			int v = (int)floorf(rgba[c] + 0.5f);
			src[c] = (v < 0) ? 0 : ((v > 255) ? 255 : v);
		}

		int z = (int)depth;
		if (z < 0) z = 0;
		if (z > 0xFFFFFF) z = 0xFFFFFF;

		int dst[4];
		SoftUnpackEfbColor(efb[idx], &dst[0], &dst[1], &dst[2], &dst[3]);

		// ---- the Z unit (gfx-pe.md 4.2) ----

		if (pe.zmode.enable)
		{
			uint32_t zref = efb[EfbZPlane + idx] & 0xFFFFFF;
			bool pass;

			switch (pe.zmode.func & 7)
			{
				case 0: pass = false; break;						// never
				case 1: pass = (uint32_t)z < zref; break;			// less
				case 2: pass = (uint32_t)z == zref; break;			// equal
				case 3: pass = (uint32_t)z <= zref; break;			// less or equal
				case 4: pass = (uint32_t)z > zref; break;			// greater
				case 5: pass = (uint32_t)z != zref; break;			// not equal
				case 6: pass = (uint32_t)z >= zref; break;			// greater or equal
				default: pass = true; break;						// always
			}

			if (!pass)
				return false;
		}

		// ---- the colour unit (gfx-pe.md 4.3) ----

		int out[4] = { src[0], src[1], src[2], src[3] };

		if (pe.cmode0.blend_en)
		{
			if (pe.cmode0.blendop)
			{
				// SUB: result = destination - source, clamped to zero (all four components)
				for (int c = 0; c < 4; c++)
				{
					int v = dst[c] - src[c];
					out[c] = (v < 0) ? 0 : v;
				}
			}
			else
			{
				for (int c = 0; c < 4; c++)
				{
					int sf = SoftBlendFactor(pe.cmode0.sfactor, true, c, src, dst);
					int df = SoftBlendFactor(pe.cmode0.dfactor, false, c, src, dst);
					int v = SoftBlendMul(dst[c], df) + SoftBlendMul(src[c], sf);
					out[c] = (v > 255) ? 255 : v;
				}
			}
		}
		else if (pe.cmode0.logop_en)
		{
			// The 16 logic operations of the colour unit (gfx-pe.md 4.3)
			for (int c = 0; c < 4; c++)
			{
				int s = src[c], d = dst[c];
				int v;

				switch (pe.cmode0.logop & 0xF)
				{
					case 0:  v = 0; break;						// clear
					case 1:  v = s & d; break;					// and
					case 2:  v = s & ~d; break;					// and_reverse
					case 3:  v = s; break;						// copy
					case 4:  v = ~s & d; break;					// and_inverted
					case 5:  v = d; break;						// noop
					case 6:  v = s ^ d; break;					// xor
					case 7:  v = s | d; break;					// or
					case 8:  v = ~(s | d); break;				// nor
					case 9:  v = ~(s ^ d); break;				// equiv
					case 10: v = ~d; break;						// invert
					case 11: v = s | ~d; break;					// or_reverse
					case 12: v = ~s; break;						// copy_inverted
					case 13: v = ~s | d; break;					// or_inverted
					case 14: v = ~(s & d); break;				// nand
					default: v = 255; break;					// set
				}

				out[c] = v & 0xFF;
			}
		}

		// The write masks gate each plane of the result, the alpha blender can be forced to write
		// the constant alpha of PE_CMODE1 (gfx-pe.md 4.3).
		if (!pe.cmode0.col_mask)
		{
			out[0] = dst[0];
			out[1] = dst[1];
			out[2] = dst[2];
		}
		if (!pe.cmode0.alpha_mask)
		{
			out[3] = dst[3];
		}
		else if (pe.cmode1.const_alpha_en)
		{
			out[3] = pe.cmode1.const_alpha & 0xFF;
		}

		efb[idx] = SoftPackEfbColor(out[0], out[1], out[2], out[3]);

		// GEN_MODE.zfreeze holds the depth of the frame (gfx-su.md 3.6)
		if (pe.zmode.mask && !(gfx != nullptr && gfx->genmode.zfreeze != 0))
			efb[EfbZPlane + idx] = (uint32_t)z;

		return true;
	}

	bool PixelEngine::SoftPixel(int x, int y, uint8_t rgba[4], uint32_t* z)
	{
		SoftAlloc();

		if (x < 0 || y < 0 || x >= soft_w || y >= soft_h)
			return false;

		size_t idx = (size_t)y * EfbStride + x;

		int r, g, b, a;
		SoftUnpackEfbColor(efb[idx], &r, &g, &b, &a);

		rgba[0] = (uint8_t)r;
		rgba[1] = (uint8_t)g;
		rgba[2] = (uint8_t)b;
		rgba[3] = (uint8_t)a;

		if (z != nullptr)
			*z = efb[EfbZPlane + idx] & 0xFFFFFF;

		return true;
	}

	//! RGB -> YCbCr of the copy engine (gfx-pe.md 5.4, studio range).
	static void SoftRgbToYuv(int r, int g, int b, int* y, int* u, int* v)
	{
		float yf = 0.257f * (float)r + 0.504f * (float)g + 0.098f * (float)b + 16.0f;
		float uf = -0.148f * (float)r - 0.291f * (float)g + 0.439f * (float)b + 128.0f;
		float vf = 0.439f * (float)r - 0.368f * (float)g - 0.071f * (float)b + 128.0f;

		*y = (int)(yf + 0.5f);
		*u = (int)(uf + 0.5f);
		*v = (int)(vf + 0.5f);

		if (*y < 0) *y = 0; if (*y > 255) *y = 255;
		if (*u < 0) *u = 0; if (*u > 255) *u = 255;
		if (*v < 0) *v = 0; if (*v > 255) *v = 255;
	}

	void PixelEngine::SoftDisplayCopy()
	{
		SoftAlloc();

		int w = (int)pe.copy_src_size.x + 1;
		int h = (int)pe.copy_src_size.y + 1;
		int srcX = (int)pe.copy_src_addr.x;
		int srcY = (int)pe.copy_src_addr.y;

		if (w <= 0 || h <= 0)
			return;

		uint32_t dstAddr = (uint32_t)pe.copy_dst_base[0].base << 5;

		// The display copy writes the XFB as packed YUV 4:2:2, four bytes per pixel pair
		// (video-interface.md 3.1, gfx-pe.md 5.6). The destination stride is programmed in 32-byte
		// units, so a 640-pixel line is 40 of them.
		size_t stride = (size_t)pe.copy_dst_stride.stride * 32;
		if (stride == 0)
			stride = (size_t)w * 2;

		uint8_t* dst = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForPI(dstAddr);
		if (dst == nullptr)
			return;

		// Gamma correction of the copy path (gfx-pe.md 5.3): 1.0, 1.7, 2.2 or bypass.
		int gammaSel = pe.copy_cmd.gamma;
		float gamma = (gammaSel == 0) ? 1.0f : ((gammaSel == 1) ? 1.7f : ((gammaSel == 2) ? 2.2f : 0.0f));

		// The vertical scaler (gfx-pe.md 5.6): the source line of an output line advances by the
		// U1.8 step of PE_COPY_SCALE and the value is a lerp between the two source lines.
		bool scale = (pe.copy_cmd.vert_scale != 0);
		float step = (float)(pe.copy_scale.scale & 0x1FF) / 256.0f;
		if (!scale || step <= 0.0f)
			step = 1.0f;

		int outLines = (int)ceilf((float)h / step);
		if (outLines < 1)
			outLines = 1;

		// The field select of an interlaced copy (gfx-pe.md 5.6): only every other destination line
		// is written.
		int fieldPhase = -1;
		if (pe.copy_cmd.interlaced == 2) fieldPhase = 0;		// even
		else if (pe.copy_cmd.interlaced == 3) fieldPhase = 1;	// odd

		std::vector<int> rgb((size_t)w * 3);
		std::vector<uint8_t> yuv((size_t)w * 2);

		for (int oy = 0; oy < outLines; oy++)
		{
			int destLine = oy;

			if (fieldPhase >= 0)
			{
				if ((oy & 1) != fieldPhase)
					continue;
				destLine = oy >> 1;
			}

			float sy = (float)oy * step;
			int y0 = (int)sy;
			int y1 = y0 + 1;
			float frac = sy - (float)y0;

			if (y0 >= h) y0 = h - 1;
			if (y1 >= h) y1 = h - 1;

			int py0 = srcY + y0;
			int py1 = srcY + y1;

			for (int x = 0; x < w; x++)
			{
				int px = srcX + x;
				if (px < 0) px = 0;
				if (px >= soft_w) px = soft_w - 1;

				int r0 = 0, g0 = 0, b0 = 0, a0 = 0;
				int r1 = 0, g1 = 0, b1 = 0, a1 = 0;
				int rr, gg, bb, aa;

				if (py0 >= 0 && py0 < soft_h)
					SoftUnpackEfbColor(efb[(size_t)py0 * EfbStride + px], &r0, &g0, &b0, &a0);

				if (py1 >= 0 && py1 < soft_h)
					SoftUnpackEfbColor(efb[(size_t)py1 * EfbStride + px], &r1, &g1, &b1, &a1);
				else
				{
					r1 = r0; g1 = g0; b1 = b0; a1 = a0;
				}

				rr = (int)((float)r0 + ((float)r1 - (float)r0) * frac + 0.5f);
				gg = (int)((float)g0 + ((float)g1 - (float)g0) * frac + 0.5f);
				bb = (int)((float)b0 + ((float)b1 - (float)b0) * frac + 0.5f);
				aa = (int)((float)a0 + ((float)a1 - (float)a0) * frac + 0.5f);

				if (gamma != 0.0f)
				{
					rr = (int)(powf((float)rr / 255.0f, gamma) * 255.0f + 0.5f);
					gg = (int)(powf((float)gg / 255.0f, gamma) * 255.0f + 0.5f);
					bb = (int)(powf((float)bb / 255.0f, gamma) * 255.0f + 0.5f);
				}

				if (rr < 0) rr = 0; if (rr > 255) rr = 255;
				if (gg < 0) gg = 0; if (gg > 255) gg = 255;
				if (bb < 0) bb = 0; if (bb > 255) bb = 255;
				if (aa < 0) aa = 0; if (aa > 255) aa = 255;

				rgb[(size_t)x * 3 + 0] = rr;
				rgb[(size_t)x * 3 + 1] = gg;
				rgb[(size_t)x * 3 + 2] = bb;
			}

			// The chroma of the 4:2:2 stream is downsampled from 4:4:4 with the ¼, ½, ¼ tent
			// filter (gfx-pe.md 5.5); the destination holds Y0 U0 Y1 V0 (video-interface.md 3.1).
			for (int x = 0; x < w; x += 2)
			{
				int l = x - 1;
				int r = x + 2;
				if (l < 0) l = 0;
				if (r > w - 1) r = w - 1;

				int y0, u0, v0, y1, u1, v1;
				SoftRgbToYuv(rgb[(size_t)x * 3 + 0], rgb[(size_t)x * 3 + 1], rgb[(size_t)x * 3 + 2], &y0, &u0, &v0);

				int x1 = (x + 1 < w) ? (x + 1) : x;
				SoftRgbToYuv(rgb[(size_t)x1 * 3 + 0], rgb[(size_t)x1 * 3 + 1], rgb[(size_t)x1 * 3 + 2], &y1, &u1, &v1);

				int ur0, ug0, ub0, ur1, ug1, ub1;
				SoftRgbToYuv(rgb[(size_t)l * 3 + 0], rgb[(size_t)l * 3 + 1], rgb[(size_t)l * 3 + 2], &ur0, &ug0, &ub0);
				SoftRgbToYuv(rgb[(size_t)r * 3 + 0], rgb[(size_t)r * 3 + 1], rgb[(size_t)r * 3 + 2], &ur1, &ug1, &ub1);

				int u = (ug0 + 2 * u0 + 2 * u1 + ug1 + 2) / 6;
				int v = (ub0 + 2 * v0 + 2 * v1 + ub1 + 2) / 6;

				yuv[(size_t)x * 2 + 0] = (uint8_t)y0;
				yuv[(size_t)x * 2 + 1] = (uint8_t)((u < 0) ? 0 : ((u > 255) ? 255 : u));
				yuv[(size_t)x * 2 + 2] = (uint8_t)y1;
				yuv[(size_t)x * 2 + 3] = (uint8_t)((v < 0) ? 0 : ((v > 255) ? 255 : v));
			}

			memcpy(dst + (size_t)destLine * stride, yuv.data(), (size_t)w * 2);
		}
	}

	//! Read a rectangle of the EFB into an RGB buffer, top row first. This is the only way back
	//! from the copy engine's round trip through the colour buffer, which is what makes it a pixel
	//! engine operation (gfx-pe.md 5). The software pipeline reads its own EFB memory, the shader
	//! pipeline the GL render target.
	bool PixelEngine::ReadEfb(int x, int y, int width, int height, std::vector<uint8_t>& rgb)
	{
		if (width <= 0 || height <= 0)
			return false;

		if (gfx != nullptr && gfx->SoftPipeline())
		{
			SoftAlloc();

			rgb.resize((size_t)width * height * 3);

			for (int row = 0; row < height; row++)
			{
				int py = y + row;

				for (int col = 0; col < width; col++)
				{
					int px = x + col;
					uint8_t* p = &rgb[((size_t)row * width + col) * 3];

					if (px < 0 || py < 0 || px >= soft_w || py >= soft_h)
					{
						p[0] = p[1] = p[2] = 0;
						continue;
					}

					int r, g, b, a;
					SoftUnpackEfbColor(efb[(size_t)py * EfbStride + px], &r, &g, &b, &a);

					p[0] = (uint8_t)r;
					p[1] = (uint8_t)g;
					p[2] = (uint8_t)b;
				}
			}

			return true;
		}

		if (gfx == nullptr || !gfx->HasGLContext())
		{
			return false;
		}

		rgb.resize((size_t)width * height * 3);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

		// glReadPixels returns the bottom row first
		std::vector<uint8_t> flipped(rgb.size());
		for (int row = 0; row < height; row++)
		{
			memcpy(&flipped[(size_t)row * width * 3],
				&rgb[(size_t)(height - 1 - row) * width * 3], (size_t)width * 3);
		}
		rgb.swap(flipped);

		return true;
	}
}
