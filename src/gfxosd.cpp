/*

The HW profiler overlay in the OpenGL back end (issue #394).

The picture itself is built by `Debug::HwOsd` (the rasterized text of the profiler report, see
hwosd.h); this file is only the OpenGL side of drawing it: one texture, one program and one quad.

The quad is generated from `gl_VertexID`, so there is no vertex buffer to keep or to bind - a
GL_TRIANGLE_STRIP of four vertices is enough, and the only state the overlay changes is the one
the frame it is drawn over has already finished with.

The overlay is drawn after the frame dump and before the buffer swap, so the dumped frames (the
ones the tests and the demo sweeps look at) stay free of debug text.

*/

#include "pch.h"

#include "gfx.h"

namespace
{

	const char* OsdVertexShader = R"glsl(
#version 330 core

uniform vec4 rect;		// x, y (top left, in NDC) and the width, height (in NDC, y downwards)

out vec2 uv;

void main()
{
	// A triangle strip of four vertices: (0,0), (1,0), (0,1), (1,1).
	vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));

	gl_Position = vec4(rect.xy + corner * rect.zw, 0.0, 1.0);

	// NDC y = +1 is the top of the frame and the picture is uploaded top row first, which is the
	// row the texture keeps at v = 0, so the two corners map straight across. (Sampling 1 - v
	// here shows the whole overlay upside down.)
	uv = corner;
}
)glsl";

	const char* OsdFragmentShader = R"glsl(
#version 330 core

uniform sampler2D osd;

in vec2 uv;

out vec4 color;

void main()
{
	color = texture(osd, uv);
}
)glsl";

	GLuint osdProgram = 0;
	GLuint osdVao = 0;
	GLuint osdTexture = 0;

	GLint osdRectUniform = -1;
	GLint osdSamplerUniform = -1;

	bool osdFailed = false;
	uint64_t osdUploadedVersion = 0;

	bool InitOsd()
	{
		if (osdProgram != 0)
			return true;

		if (osdFailed)
			return false;

		GLuint vertex = GFX::CompileShaderStage(GL_VERTEX_SHADER, OsdVertexShader, "OSD vertex");
		if (vertex == 0)
		{
			osdFailed = true;
			return false;
		}

		GLuint fragment = GFX::CompileShaderStage(GL_FRAGMENT_SHADER, OsdFragmentShader, "OSD fragment");
		if (fragment == 0)
		{
			glDeleteShader(vertex);
			osdFailed = true;
			return false;
		}

		osdProgram = glCreateProgram();
		glAttachShader(osdProgram, vertex);
		glAttachShader(osdProgram, fragment);
		glLinkProgram(osdProgram);

		glDeleteShader(vertex);
		glDeleteShader(fragment);

		GLint linked = 0;
		glGetProgramiv(osdProgram, GL_LINK_STATUS, &linked);
		if (linked == 0)
		{
			Debug::Report(Debug::Channel::Error, "GFX: cannot link the OSD program\n");
			glDeleteProgram(osdProgram);
			osdProgram = 0;
			osdFailed = true;
			return false;
		}

		osdRectUniform = glGetUniformLocation(osdProgram, "rect");
		osdSamplerUniform = glGetUniformLocation(osdProgram, "osd");

		// The quad comes from gl_VertexID, but a core profile still insists on a bound VAO.
		glGenVertexArrays(1, &osdVao);

		glGenTextures(1, &osdTexture);
		glBindTexture(GL_TEXTURE_2D, osdTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		return true;
	}

}

namespace GFX
{

	void OsdDraw()
	{
		// Pull the report through the debug interface and let it rasterize the picture when the
		// refresh interval has passed. It is a no-op while the overlay is off.
		Debug::HwOsd::Update();

		std::vector<uint8_t> rgba;
		int width = 0, height = 0;

		if (!Debug::HwOsd::CopyImage(rgba, &width, &height))
			return;

		if (width <= 0 || height <= 0)
			return;

		if (!InitOsd())
			return;

		// The overlay belongs to the picture, not to the window: the picture is scaled into the
		// largest rectangle of the window that has its shape (the window is the user's, issue #458),
		// so the overlay is drawn into that rectangle and is scaled with it. The software pipeline
		// does the same thing by blitting it into the XFB the video interface shows (see
		// HwOsd::Blit), which is what keeps the two back ends alike.
		GFXCore* core = (Flipper::HW != nullptr) ? Flipper::HW->gfx : nullptr;

		if (core == nullptr)
			return;

		const GFXCore::Presentation& presented = core->presentation;

		if (presented.source == 0 || presented.windowWidth <= 0 || presented.windowHeight <= 0)
			return;

		const int sourceWidth = (int)core->RenderWidth();
		const int sourceHeight = (int)core->RenderHeight();

		// The overlay covers the picture with one texel of its own per pixel of the source, so it is
		// scaled into the window by the same factor the picture was.
		const float scale = (float)presented.width / (float)sourceWidth;

		// A picture that does not fit in the corner is clamped rather than scaled (an overlay that
		// changes size with the window is harder to read than one that is cut off).
		int drawWidth = (width < sourceWidth) ? width : sourceWidth;
		int drawHeight = (height < sourceHeight) ? height : sourceHeight;

		uint64_t version = Debug::HwOsd::Version();

		glBindTexture(GL_TEXTURE_2D, osdTexture);

		if (version != osdUploadedVersion)
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
			osdUploadedVersion = version;
		}

		// The frame is complete by now (the scene, the clears and the dumps are done), so the state
		// the overlay has to put back is the blend it turns on and the viewport it changes.
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		// The overlay is drawn in the coordinates of the window, so the whole of the window is the
		// viewport while it is drawn: the pipeline leaves the console's own viewport behind (a title
		// programs it through XF_VIEWPORT), and the NDC rectangle below is measured against the
		// window - a viewport of the console's size would scale and move it (the overlay used to
		// land on the top left corner of the *console's* picture, which was the same thing only
		// while the window and the render target were the same size).
		glViewport(0, 0, presented.windowWidth, presented.windowHeight);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_COLOR_LOGIC_OP);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(osdProgram);
		glBindVertexArray(osdVao);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, osdTexture);
		glUniform1i(osdSamplerUniform, 0);

		// The rectangle of the picture the overlay goes into, in the pixels of the window, mapped
		// onto the NDC rectangle the viewport above makes the whole of it.
		const float windowWidth = (float)presented.windowWidth;
		const float windowHeight = (float)presented.windowHeight;

		float x0 = 2.0f * (float)presented.x / windowWidth - 1.0f;
		float y0 = 1.0f - 2.0f * (float)presented.y / windowHeight;
		float w = 2.0f * (float)drawWidth * scale / windowWidth;
		float h = -2.0f * (float)drawHeight * scale / windowHeight;

		glUniform4f(osdRectUniform, x0, y0, w, h);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glBindVertexArray(0);
		glUseProgram(0);
		glDisable(GL_BLEND);

		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	}

	void OsdDispose()
	{
		if (osdTexture != 0)
		{
			glDeleteTextures(1, &osdTexture);
			osdTexture = 0;
		}

		if (osdVao != 0)
		{
			glDeleteVertexArrays(1, &osdVao);
			osdVao = 0;
		}

		if (osdProgram != 0)
		{
			glDeleteProgram(osdProgram);
			osdProgram = 0;
		}

		osdRectUniform = -1;
		osdSamplerUniform = -1;
		osdUploadedVersion = 0;
		osdFailed = false;
	}

}
