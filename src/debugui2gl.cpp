/*

# DebugUI2 - the GL front end

The reference implementation of `Debug2::Ui`: a separate window with an OpenGL 3.3 context that
draws the panel tree the debugger builds.

There is no UI toolkit behind this. The window is an SDL window, the text comes out of a glyph
atlas built at start-up from `Data/DebugUiMono.ttf` with `stb_truetype`, and the panels, the
scrolling and the command line are laid out by this module.

The layout is a tree of panels. A panel is a frame with a header (the title, and to the right of
it the info line the debugger put there) and a body that holds either the queued items, a split,
or a tab strip. The panel under the pointer is the one with the focus: it is drawn with a thicker
border and, when its content does not fit, with a vertical scrollbar (the other panels keep their
full width). The scroll position of a panel and the selected tab are kept here, per panel, rather
than in the debugger, which only publishes what is to be drawn.

## Text on GL

The atlas is one 8-bit texture: `stb_truetype` rasterizes the codepoints of the ranges listed in
`AtlasRanges` into it, and the text is drawn as a batch of textured quads. Everything else - the
panel frames, the caret, the rules - is the same batch with the "solid" mode, so a frame is a
handful of draw calls at most.

The `y` a line of text is drawn at is the top of the line; a glyph quad is placed against the
baseline, which is what `stbtt_GetPackedQuad` expects, so `DrawText` adds the ascent itself.

The atlas carries more than ASCII on purpose. The JDI interface is ASCII for now (UTF-8 is a
separate task), but the renderer should not have to be rewritten when that changes: the ranges
cover Latin, Greek, Cyrillic, the box drawing and block elements, arrows, mathematical operators
and the common symbols.

## Threading

A GL context belongs to the thread that created it, and SDL wants its events pumped from one
place, so the window is created and drawn from the host UI thread. The host calls
`Debug2::Frame` once per frame and passes the events it pumps to `Debug2::UiSdlEvent` (a host
without an event loop of its own calls `Debug2::UiPumpSdlEvents` instead). The debugger itself -
the session, the commands, the panels - lives in its own thread and hands the front end a
finished snapshot, so the renderer never waits for the debugger and the debugger never waits for
the renderer.

*/

#include "pch.h"

#include <cstddef>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Debug2
{

	// ----------------------------------------------------------------------------------------
	// Colors (a dark theme)
	// ----------------------------------------------------------------------------------------

	static const uint32_t ColWindowBg = 0xFF1E1E1E;
	static const uint32_t ColPanelBg = 0xFF252526;
	static const uint32_t ColPanelBorder = 0xFF3C3C3C;
	static const uint32_t ColPanelFocus = 0xFF007ACC;	// the border of the panel under the pointer
	static const uint32_t ColPanelTitleBg = 0xFF2D2D30;
	static const uint32_t ColPanelTitle = 0xFFB0B0B0;
	static const uint32_t ColTabHover = 0xFF3F3F46;
	static const uint32_t ColScrollTrack = 0xFF1B1B1B;
	static const uint32_t ColScrollThumb = 0xFF4E4E4E;
	static const uint32_t ColText = 0xFFD4D4D4;
	static const uint32_t ColCode = 0xFFCE9178;
	static const uint32_t ColHeading = 0xFF4EC9B0;
	static const uint32_t ColStrong = 0xFFFFFFFF;
	static const uint32_t ColEmphasis = 0xFFB8C4D0;
	static const uint32_t ColRule = 0xFF555555;
	static const uint32_t ColImage = 0xFF569CD6;
	static const uint32_t ColCmdBg = 0xFF141414;
	static const uint32_t ColCmdText = 0xFFFFFFFF;
	static const uint32_t ColCmdHint = 0xFF6A6A6A;

	static void UnpackColor(uint32_t color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
	{
		// 0xAARRGGBB
		*r = (uint8_t)(color >> 16);
		*g = (uint8_t)(color >> 8);
		*b = (uint8_t)(color);
		*a = (uint8_t)(color >> 24);
	}

	// The codepoint ranges packed into the atlas.
	static const struct
	{
		int first;
		int count;
	} AtlasRanges[] =
	{
		{ 0x0020, 0x0060 },		// ASCII, printable
		{ 0x00A0, 0x0100 },		// Latin-1 supplement, Latin Extended-A
		{ 0x0100, 0x0080 },		// Latin Extended-A/B
		{ 0x02B0, 0x0050 },		// Spacing modifiers
		{ 0x0370, 0x0090 },		// Greek
		{ 0x0400, 0x0100 },		// Cyrillic
		{ 0x2010, 0x0050 },		// General punctuation
		{ 0x2070, 0x0030 },		// Super- and subscripts
		{ 0x20A0, 0x0040 },		// Currency
		{ 0x2100, 0x0060 },		// Letterlike symbols, number forms
		{ 0x2190, 0x0100 },		// Arrows
		{ 0x2200, 0x0100 },		// Mathematical operators
		{ 0x2300, 0x0060 },		// Miscellaneous technical
		{ 0x2500, 0x0100 },		// Box drawing, block elements
		{ 0x25A0, 0x0060 },		// Geometric shapes
		{ 0x2600, 0x0080 },		// Miscellaneous symbols
		{ 0x2700, 0x0080 },		// Dingbats
	};

	struct GlyphRange
	{
		int first = 0;
		int count = 0;
		int offset = 0;
	};

	// What the fragment shader is to do with the sampled texel.
	enum class TexMode
	{
		Glyph = 0,			// the red channel is the coverage of the glyph
		Solid,				// the color alone (frames, rules, the caret)
		Image,				// the texel is RGBA
	};

	struct Vertex
	{
		float x, y;
		float u, v;
		float mode;
		uint8_t r, g, b, a;
	};

	static const char* VertexShaderSource =
		"#version 330 core\n"
		"layout(location = 0) in vec2 aPos;\n"
		"layout(location = 1) in vec2 aUV;\n"
		"layout(location = 2) in float aMode;\n"
		"layout(location = 3) in vec4 aColor;\n"
		"uniform vec2 uViewport;\n"
		"out vec2 vUV;\n"
		"out float vMode;\n"
		"out vec4 vColor;\n"
		"void main()\n"
		"{\n"
		"    vUV = aUV;\n"
		"    vMode = aMode;\n"
		"    vColor = aColor;\n"
		"    gl_Position = vec4(aPos.x / uViewport.x * 2.0 - 1.0, 1.0 - aPos.y / uViewport.y * 2.0, 0.0, 1.0);\n"
		"}\n";

	static const char* FragmentShaderSource =
		"#version 330 core\n"
		"in vec2 vUV;\n"
		"in float vMode;\n"
		"in vec4 vColor;\n"
		"uniform sampler2D uTexture;\n"
		"out vec4 FragColor;\n"
		"void main()\n"
		"{\n"
		"    vec4 texel = texture(uTexture, vUV);\n"
		"    if (vMode < 0.5)\n"
		"        FragColor = vec4(vColor.rgb, vColor.a * texel.r);\n"		// a glyph
		"    else if (vMode < 1.5)\n"
		"        FragColor = vColor;\n"										// a solid rectangle
		"    else\n"
		"        FragColor = vec4(texel.rgb, vColor.a * texel.a);\n"		// an image
		"}\n";

	static GLuint CompileShader(GLenum type, const char* source)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		GLint ok = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			char log[0x400] = { 0, };
			glGetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
			Debug::Report(Debug::Channel::Error, "debugui2: shader: %s\n", log);
			glDeleteShader(shader);
			return 0;
		}

		return shader;
	}

	struct Rect
	{
		float x = 0, y = 0, w = 0, h = 0;

		float Right() const { return x + w; }
		float Bottom() const { return y + h; }
		bool Contains(float px, float py) const { return px >= x && px < Right() && py >= y && py < Bottom(); }
	};

	struct Run
	{
		MdStyle style = MdStyle::Norm;
		std::string text;
	};

	// One line as it ends up on the screen: the runs of a Markdown line, already wrapped.
	struct VisualLine
	{
		std::vector<Run> runs;
		float height = 0;
		bool image = false;
		std::string ref;
		float imageWidth = 0;
		float imageHeight = 0;
	};

	struct PanelState
	{
		float scroll = 0;
		bool stickToEnd = false;
		size_t lastItemCount = 0;
		size_t activeTab = 0;
	};

	// The geometry of a scrollbar, kept from the layout so that a mouse event can hit it.
	struct ScrollGeom
	{
		Rect track;
		Rect thumb;
		float maxScroll = 0;
		std::string key;
	};

	// The rectangle of one tab, kept from the layout so that a click can be routed to it.
	struct TabHit
	{
		Rect rect;
		std::string key;
		size_t index = 0;
	};

	struct ImageTexture
	{
		GLuint texture = 0;
		int width = 0;
		int height = 0;
		bool loaded = false;
	};

	// The scrollbar lives in the right edge of a leaf panel; the text is wrapped that much
	// narrower, whether the bar is shown or not, so that gaining the focus does not reflow it.
	static const float ScrollbarWidth = 6.0f;
	static const float ScrollbarGap = 3.0f;
	static const float TabPadding = 7.0f;


	// ----------------------------------------------------------------------------------------
	// The front end
	// ----------------------------------------------------------------------------------------

	class GlUi : public Ui
	{
		SDL_Window* window = nullptr;
		SDL_GLContext context = nullptr;
		Uint32 windowId = 0;
		bool open = false;
		bool sdlOwned = false;

		Sink* sink = nullptr;
		std::string sessionPath;

		GLuint program = 0;
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint atlas = 0;
		GLint viewportUniform = -1;

		// The font
		std::vector<uint8_t> fontData;
		stbtt_fontinfo font = { 0 };
		bool hasFont = false;

		std::vector<stbtt_packedchar> packed;
		std::vector<GlyphRange> ranges;
		int atlasWidth = 0;
		int atlasHeight = 0;
		float fontSize = 13.0f;
		float lineHeight = 0;
		float ascent = 0;				// the baseline of a line, from the top of the line
		float advance[128] = { 0 };
		float defaultAdvance = 8;

		// The frame being built
		std::vector<Vertex> vertices;
		int viewportWidth = 0;
		int viewportHeight = 0;
		GLuint batchTexture = 0;

		// The command line
		std::string cmdline;
		size_t cursor = 0;
		int historyPos = -1;
		std::vector<std::string> cmdHistory;

		std::map<std::string, PanelState> panelStates;	// per panel, keyed by its path in the tree
		std::map<const Panel*, Rect> panelRects;		// of the frame being drawn, for the hit tests
		std::map<const Panel*, std::string> panelKeys;
		std::map<const Panel*, ScrollGeom> scrollGeoms;
		std::vector<TabHit> tabHits;
		std::map<std::string, ImageTexture> images;

		// The view of the last frame. It is kept alive so that a mouse event can walk the tree
		// that was drawn: the debugger thread replaces the snapshot whenever it likes.
		std::shared_ptr<const View> renderedView;
		const Panel* hoveredPanel = nullptr;

		// A scrollbar drag in progress.
		const Panel* dragPanel = nullptr;
		float dragOffset = 0;

		float mouseX = 0, mouseY = 0;

		// ---- font ----

		bool BuildFont();
		bool PackRanges(const std::vector<uint8_t>& data, stbtt_fontinfo* info,
			std::vector<stbtt_packedchar>& outPacked, std::vector<GlyphRange>& outRanges,
			uint8_t* bitmap, int width, int height);
		stbtt_packedchar* FindGlyph(int codepoint);
		float CharAdvance(int codepoint);
		float MeasureText(const std::string& text);

		// ---- the batch ----

		void BeginBatch(int width, int height);
		void FlushBatch();
		void EmitQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
			TexMode mode, uint32_t color);
		void FillRect(const Rect& rect, uint32_t color);
		void FrameRect(const Rect& rect, uint32_t color, float thickness);
		void DrawText(float x, float y, const std::string& text, uint32_t color);
		void DrawRuns(float x, float y, const std::vector<Run>& runs);

		// ---- the layout ----

		std::vector<VisualLine> BuildItemLines(const Item& item, float maxWidth);
		float LinesHeight(const std::vector<VisualLine>& lines) const;
		void DrawLines(const std::vector<VisualLine>& lines, const Rect& content, float y, ItemAlign align);
		void DrawImageBlock(const VisualLine& line, const Rect& content, float y);
		ImageTexture& GetImage(const std::string& ref);
		void LayoutPanel(const Panel& panel, const Rect& rect, const std::string& key, bool showTitle = true);
		void LayoutTabs(const Panel& panel, const Rect& content, const std::string& key);
		const Panel* HitTest(const Panel& panel, float x, float y) const;
		void UpdateHover();
		void DrawScrollbar(const ScrollGeom& geom);

		// ---- the input ----

		void InsertText(const char* utf8);
		void HandleSdlKey(const SDL_KeyboardEvent& key);
		void MouseDown(float x, float y);
		void MouseUp();
		void DragScrollbar(float y);
		void HandleSdlEvent(const SDL_Event& event);

	public:
		GlUi() = default;
		virtual ~GlUi() { Close(); }

		virtual bool Open(const std::string& title, Sink* sink);
		virtual void Close();
		virtual bool IsOpen() const { return open; }
		virtual void Render(const std::shared_ptr<const View>& view);

		bool HandleEvent(const SDL_Event& event);
	};

	static GlUi* g_GlUi = nullptr;


	// ----------------------------------------------------------------------------------------
	// The font
	// ----------------------------------------------------------------------------------------

	bool GlUi::PackRanges(const std::vector<uint8_t>& data, stbtt_fontinfo* info,
		std::vector<stbtt_packedchar>& outPacked, std::vector<GlyphRange>& outRanges,
		uint8_t* bitmap, int width, int height)
	{
		(void)info;

		outPacked.clear();
		outRanges.clear();

		stbtt_pack_context pc;

		if (!stbtt_PackBegin(&pc, bitmap, width, height, 0, 1, nullptr))
			return false;

		// The text is small, and this is what keeps it readable.
		stbtt_PackSetOversampling(&pc, 2, 2);

		int offset = 0;

		for (int i = 0; i < _countof(AtlasRanges); i++)
		{
			int first = AtlasRanges[i].first;
			int count = AtlasRanges[i].count;

			outPacked.resize(offset + count);

			if (!stbtt_PackFontRange(&pc, data.data(), 0, fontSize, first, count, &outPacked[offset]))
			{
				stbtt_PackEnd(&pc);
				return false;
			}

			GlyphRange range;
			range.first = first;
			range.count = count;
			range.offset = offset;
			outRanges.push_back(range);

			offset += count;
		}

		stbtt_PackEnd(&pc);
		return true;
	}

	bool GlUi::BuildFont()
	{
		fontData = Util::FileLoad("Data/DebugUiMono.ttf");

		if (fontData.empty())
		{
			Debug::Report(Debug::Channel::Error, "debugui2: Data/DebugUiMono.ttf is missing, the debugger window would have no text\n");
			return false;
		}

		if (!stbtt_InitFont(&font, fontData.data(), stbtt_GetFontOffsetForIndex(fontData.data(), 0)))
		{
			Debug::Report(Debug::Channel::Error, "debugui2: Data/DebugUiMono.ttf is not a TrueType font\n");
			fontData.clear();
			return false;
		}

		int ascentRaw = 0, descentRaw = 0, lineGapRaw = 0;
		stbtt_GetFontVMetrics(&font, &ascentRaw, &descentRaw, &lineGapRaw);

		float scale = stbtt_ScaleForPixelHeight(&font, fontSize);
		lineHeight = ceilf((ascentRaw - descentRaw + lineGapRaw) * scale) + 1.0f;
		ascent = ascentRaw * scale;

		// The advances are cached. The font is monospaced, but the wrapping does not assume it.
		defaultAdvance = CharAdvance('?');
		for (int c = 0; c < 128; c++)
			advance[c] = CharAdvance(c);

		// Pack into the smallest sheet the ranges fit into.
		const int sizes[] = { 1024, 2048, 4096 };
		std::vector<uint8_t> bitmap;

		for (size_t attempt = 0; attempt < _countof(sizes); attempt++)
		{
			int size = sizes[attempt];
			bitmap.assign((size_t)size * size, 0);

			if (PackRanges(fontData, &font, packed, ranges, bitmap.data(), size, size))
			{
				atlasWidth = size;
				atlasHeight = size;
				break;
			}

			packed.clear();
			ranges.clear();
		}

		if (ranges.empty())
		{
			Debug::Report(Debug::Channel::Error, "debugui2: the glyph atlas does not fit into a sheet, the debugger window would have no text\n");
			return false;
		}

		glGenTextures(1, &atlas);
		glBindTexture(GL_TEXTURE_2D, atlas);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		hasFont = true;
		return true;
	}

	stbtt_packedchar* GlUi::FindGlyph(int codepoint)
	{
		for (size_t i = 0; i < ranges.size(); i++)
		{
			if (codepoint >= ranges[i].first && codepoint < (ranges[i].first + ranges[i].count))
				return &packed[ranges[i].offset + (codepoint - ranges[i].first)];
		}

		return nullptr;
	}

	float GlUi::CharAdvance(int codepoint)
	{
		int glyph = stbtt_FindGlyphIndex(&font, codepoint);
		int adv = 0, lsb = 0;
		stbtt_GetGlyphHMetrics(&font, glyph, &adv, &lsb);
		return adv * stbtt_ScaleForPixelHeight(&font, fontSize);
	}

	float GlUi::MeasureText(const std::string& text)
	{
		float width = 0;

		for (size_t i = 0; i < text.size(); i++)
		{
			unsigned char c = (unsigned char)text[i];
			width += (c < 128) ? advance[c] : defaultAdvance;
		}

		return width;
	}


	// ----------------------------------------------------------------------------------------
	// The batch
	// ----------------------------------------------------------------------------------------

	void GlUi::BeginBatch(int width, int height)
	{
		viewportWidth = width;
		viewportHeight = height;
		vertices.clear();
		batchTexture = atlas;
	}

	void GlUi::FlushBatch()
	{
		if (vertices.empty())
			return;

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);

		glUseProgram(program);
		glUniform2f(viewportUniform, (float)viewportWidth, (float)viewportHeight);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, batchTexture);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());

		vertices.clear();
	}

	void GlUi::EmitQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
		TexMode mode, uint32_t color)
	{
		uint8_t r, g, b, a;
		UnpackColor(color, &r, &g, &b, &a);

		Vertex v[4];
		v[0] = { x0, y0, u0, v0, (float)mode, r, g, b, a };
		v[1] = { x1, y0, u1, v0, (float)mode, r, g, b, a };
		v[2] = { x1, y1, u1, v1, (float)mode, r, g, b, a };
		v[3] = { x0, y1, u0, v1, (float)mode, r, g, b, a };

		vertices.push_back(v[0]);
		vertices.push_back(v[1]);
		vertices.push_back(v[2]);

		vertices.push_back(v[0]);
		vertices.push_back(v[2]);
		vertices.push_back(v[3]);
	}

	void GlUi::FillRect(const Rect& rect, uint32_t color)
	{
		EmitQuad(rect.x, rect.y, rect.Right(), rect.Bottom(), 0, 0, 0, 0, TexMode::Solid, color);
	}

	void GlUi::FrameRect(const Rect& rect, uint32_t color, float thickness)
	{
		FillRect({ rect.x, rect.y, rect.w, thickness }, color);
		FillRect({ rect.x, rect.Bottom() - thickness, rect.w, thickness }, color);
		FillRect({ rect.x, rect.y, thickness, rect.h }, color);
		FillRect({ rect.Right() - thickness, rect.y, thickness, rect.h }, color);
	}

	void GlUi::DrawText(float x, float y, const std::string& text, uint32_t color)
	{
		// `y` is the top of the line; stbtt_GetPackedQuad() places a glyph against the baseline.
		float baseline = y + ascent;

		for (size_t i = 0; i < text.size(); i++)
		{
			unsigned char c = (unsigned char)text[i];

			stbtt_packedchar* glyph = (c < 128) ? FindGlyph(c) : nullptr;

			if (glyph != nullptr)
			{
				stbtt_aligned_quad q;
				float px = x, py = baseline;
				// `glyph` is already the entry of this codepoint: stbtt_GetPackedQuad() indexes the
				// array it is given by the character index, so the index here is 0.
				stbtt_GetPackedQuad(glyph, atlasWidth, atlasHeight, 0, &px, &py, &q, 0);
				EmitQuad(q.x0, q.y0, q.x1, q.y1, q.s0, q.t0, q.s1, q.t1, TexMode::Glyph, color);
			}

			x += (c < 128) ? advance[c] : defaultAdvance;
		}
	}

	void GlUi::DrawRuns(float x, float y, const std::vector<Run>& runs)
	{
		for (size_t i = 0; i < runs.size(); i++)
		{
			const Run& run = runs[i];

			uint32_t color = ColText;

			switch (run.style)
			{
				case MdStyle::Strong: color = ColStrong; break;
				case MdStyle::Emphasis: color = ColEmphasis; break;
				case MdStyle::Code: color = ColCode; break;
				case MdStyle::Heading: color = ColHeading; break;
				default: break;
			}

			DrawText(x, y, run.text, color);
			x += MeasureText(run.text);
		}
	}


	// ----------------------------------------------------------------------------------------
	// The layout
	// ----------------------------------------------------------------------------------------

	float GlUi::LinesHeight(const std::vector<VisualLine>& lines) const
	{
		float height = 0;
		for (size_t i = 0; i < lines.size(); i++)
			height += lines[i].height;
		return height;
	}

	ImageTexture& GlUi::GetImage(const std::string& ref)
	{
		auto it = images.find(ref);
		if (it != images.end())
			return it->second;

		ImageTexture tex;

		// The root for the pictures is the session folder.
		std::string path = sessionPath.empty() ? ref : (sessionPath + "/" + ref);

		int width = 0, height = 0, channels = 0;
		stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);

		if (pixels != nullptr)
		{
			glGenTextures(1, &tex.texture);
			glBindTexture(GL_TEXTURE_2D, tex.texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			stbi_image_free(pixels);

			tex.width = width;
			tex.height = height;
			tex.loaded = true;
		}

		glBindTexture(GL_TEXTURE_2D, batchTexture);

		return images.insert(std::make_pair(ref, tex)).first->second;
	}

	// The lines one Markdown item turns into at this width. An image is a block of its own, so it
	// is not wrapped together with the text around it.
	std::vector<VisualLine> GlUi::BuildItemLines(const Item& item, float maxWidth)
	{
		std::vector<VisualLine> lines;

		if (maxWidth < 8)
			maxWidth = 8;

		for (size_t l = 0; l < item.view.size(); l++)
		{
			const MdLine& mdLine = item.view[l];

			if (mdLine.spans.empty())
			{
				// A paragraph break: half a line of air.
				VisualLine blank;
				blank.height = lineHeight * 0.5f;
				lines.push_back(blank);
				continue;
			}

			// The images of the line come first, each on a line of its own.
			for (size_t s = 0; s < mdLine.spans.size(); s++)
			{
				if (mdLine.spans[s].style != MdStyle::Image)
					continue;

				VisualLine image;
				image.image = true;
				image.ref = mdLine.spans[s].ref;

				ImageTexture& tex = GetImage(image.ref);
				if (tex.loaded)
				{
					image.imageWidth = my_min((float)tex.width, maxWidth);
					image.imageHeight = tex.height * (image.imageWidth / (float)tex.width);
				}
				else
				{
					image.imageWidth = my_min(maxWidth, 320.0f);
					image.imageHeight = lineHeight + 4;
				}

				image.height = image.imageHeight + 4;
				lines.push_back(image);
			}

			// A horizontal rule is a line of its own.
			if (mdLine.spans[0].style == MdStyle::Rule)
			{
				VisualLine rule;
				rule.height = lineHeight * 0.5f;
				lines.push_back(rule);
				continue;
			}

			// The text: a greedy wrap, breaking at the last space that fits and cutting a word
			// that does not fit on a line of its own.
			std::vector<Run> current;
			float x = 0;
			bool anything = false;

			for (size_t s = 0; s < mdLine.spans.size(); s++)
			{
				if (mdLine.spans[s].style == MdStyle::Image)
					continue;

				const std::string& text = mdLine.spans[s].text;

				size_t wordBegin = 0;

				for (size_t i = 0; i <= text.size(); i++)
				{
					bool atEnd = (i == text.size());
					bool atSpace = (!atEnd && text[i] == ' ');

					if (!atEnd && !atSpace)
						continue;

					std::string word = text.substr(wordBegin, i - wordBegin);
					wordBegin = i + 1;

					if (word.empty() && !atSpace)
						continue;

					float wordWidth = MeasureText(word);

					if (!current.empty() && (x + wordWidth) > maxWidth)
					{
						VisualLine wrapped;
						wrapped.runs = current;
						wrapped.height = lineHeight;
						lines.push_back(wrapped);

						current.clear();
						x = 0;

						if (atSpace)
							continue;		// the space that caused the break is swallowed
					}

					// A word longer than the whole line is cut into pieces that fit.
					if (wordWidth > maxWidth)
					{
						std::string piece;
						float pieceWidth = 0;

						for (size_t c = 0; c < word.size(); c++)
						{
							float cw = MeasureText(word.substr(c, 1));

							if (!piece.empty() && (pieceWidth + cw) > maxWidth)
							{
								Run part;
								part.style = mdLine.spans[s].style;
								part.text = piece;
								current.push_back(part);
								x += pieceWidth;

								VisualLine wrapped;
								wrapped.runs = current;
								wrapped.height = lineHeight;
								lines.push_back(wrapped);

								current.clear();
								x = 0;
								piece.clear();
								pieceWidth = 0;
							}

							piece += word[c];
							pieceWidth += cw;
						}

						if (!piece.empty())
						{
							Run part;
							part.style = mdLine.spans[s].style;
							part.text = piece;
							current.push_back(part);
							x += pieceWidth;
						}

						anything = true;
						continue;
					}

					Run part;
					part.style = mdLine.spans[s].style;
					part.text = word;
					current.push_back(part);
					x += wordWidth;
					anything = true;

					if (atSpace)
					{
						Run space;
						space.style = mdLine.spans[s].style;
						space.text = " ";
						current.push_back(space);
						x += MeasureText(" ");
					}
				}
			}

			if (!current.empty())
			{
				VisualLine wrapped;
				wrapped.runs = current;
				wrapped.height = lineHeight;
				lines.push_back(wrapped);
			}
			else if (!anything)
			{
				lines.push_back(VisualLine{ {}, lineHeight });
			}
		}

		return lines;
	}

	void GlUi::DrawImageBlock(const VisualLine& line, const Rect& content, float y)
	{
		ImageTexture& tex = GetImage(line.ref);

		if (!tex.loaded)
		{
			Rect box = { content.x, y, my_min(content.w, 320.0f), line.imageHeight };
			FrameRect(box, ColImage, 1.0f);
			DrawText(box.x + 4, box.y + 2, "image not found: " + line.ref, ColImage);
			return;
		}

		// The pictures live in their own textures, so the batch is flushed and rebound around
		// them (the frame is at most a few draw calls per picture).
		FlushBatch();
		batchTexture = tex.texture;
		EmitQuad(content.x, y, content.x + line.imageWidth, y + line.imageHeight,
			0, 0, 1, 1, TexMode::Image, 0xFFFFFFFF);
		FlushBatch();
		batchTexture = atlas;
	}

	void GlUi::DrawLines(const std::vector<VisualLine>& lines, const Rect& content, float y, ItemAlign align)
	{
		for (size_t i = 0; i < lines.size(); i++)
		{
			const VisualLine& line = lines[i];

			if (line.image)
			{
				DrawImageBlock(line, content, y);
			}
			else if (line.runs.size() == 1 && line.runs[0].style == MdStyle::Rule)
			{
				FillRect({ content.x, y + line.height * 0.5f, content.w, 1.0f }, ColRule);
			}
			else if (!line.runs.empty())
			{
				float width = 0;
				for (size_t r = 0; r < line.runs.size(); r++)
					width += MeasureText(line.runs[r].text);

				float x = (align == ItemAlign::Right) ? (content.Right() - width) : content.x;

				DrawRuns(x, y, line.runs);
			}

			y += line.height;
		}
	}

	// The deepest panel that contains the point, or nullptr. The walk goes over the snapshot the
	// last frame was drawn from, and the deepest hit is the panel with the focus.
	const Panel* GlUi::HitTest(const Panel& panel, float x, float y) const
	{
		auto it = panelRects.find(&panel);

		if (it == panelRects.end() || !it->second.Contains(x, y))
			return nullptr;

		for (size_t i = 0; i < panel.SubCount(); i++)
		{
			const Panel* hit = HitTest(panel.Sub(i), x, y);
			if (hit != nullptr)
				return hit;
		}

		return &panel;
	}

	void GlUi::UpdateHover()
	{
		// While a scrollbar is dragged the focus stays on its panel, even if the pointer wanders
		// off it (the thumb is following the pointer).
		if (dragPanel != nullptr)
		{
			hoveredPanel = dragPanel;
			return;
		}

		hoveredPanel = (renderedView != nullptr) ? HitTest(renderedView->root, mouseX, mouseY) : nullptr;
	}

	void GlUi::DrawScrollbar(const ScrollGeom& geom)
	{
		FillRect(geom.track, ColScrollTrack);

		if (geom.thumb.h > 0)
			FillRect(geom.thumb, ColScrollThumb);
	}

	void GlUi::LayoutPanel(const Panel& panel, const Rect& rect, const std::string& key, bool showTitle)
	{
		if (rect.w < 6 || rect.h < 6)
			return;

		FillRect(rect, ColPanelBg);
		FrameRect(rect, ColPanelBorder, 1.0f);

		// The body starts under the header. A panel that is drawn as the active tab of another
		// one has no header of its own: the tab strip already carries the title.
		float top = rect.y;

		if (showTitle)
		{
			Rect title = { rect.x + 1, rect.y + 1, rect.w - 2, lineHeight + 2 };

			if (!panel.Title().empty() || !panel.Info().empty())
			{
				FillRect(title, ColPanelTitleBg);

				float titleWidth = MeasureText(panel.Title());
				if (!panel.Title().empty())
					DrawText(title.x + 4, title.y + 1, panel.Title(), ColPanelTitle);

				// The info goes to the right edge, as long as it does not run into the title.
				if (!panel.Info().empty())
				{
					float infoX = title.Right() - 4 - MeasureText(panel.Info());
					if (infoX > title.x + 8 + titleWidth)
						DrawText(infoX, title.y + 1, panel.Info(), ColPanelTitle);
				}
			}

			top = title.Bottom();
		}

		Rect content = { rect.x + 3, top + 1, rect.w - 6, rect.Bottom() - top - 4 };

		if (content.w < 4 || content.h < 4)
			return;

		panelRects[&panel] = rect;
		panelKeys[&panel] = key;

		if (panel.GetSplit() == Split::Tabs)
		{
			LayoutTabs(panel, content, key);
			return;
		}

		if (panel.GetSplit() != Split::None)
		{
			size_t count = panel.SubCount();
			if (count == 0)
				return;

			float gap = 2.0f;

			if (panel.GetSplit() == Split::Vertical)
			{
				float each = (content.w - gap * (count - 1)) / (float)count;

				for (size_t i = 0; i < count; i++)
					LayoutPanel(panel.Sub(i), { content.x + i * (each + gap), content.y, each, content.h },
						key + "/" + std::to_string(i));
			}
			else
			{
				float each = (content.h - gap * (count - 1)) / (float)count;

				for (size_t i = 0; i < count; i++)
					LayoutPanel(panel.Sub(i), { content.x, content.y + i * (each + gap), content.w, each },
						key + "/" + std::to_string(i));
			}

			return;
		}

		// A leaf panel. The command line, if the panel has one, lives at the bottom.
		Rect itemArea = content;
		Rect cmdArea = { 0, 0, 0, 0 };
		bool hasCmdline = panel.HasCmdline() && sink != nullptr;

		if (hasCmdline)
		{
			float cmdHeight = lineHeight + 6;
			cmdArea = { content.x, content.Bottom() - cmdHeight, content.w, cmdHeight };
			itemArea.h -= (cmdHeight + 2);
		}

		// The scrollbar column is kept free whether the bar is drawn or not, so that gaining
		// the focus does not reflow the text.
		Rect textArea = itemArea;
		textArea.w = my_max(8.0f, textArea.w - (ScrollbarWidth + ScrollbarGap));

		// Lay the items out once: the same lines are measured for the scroll and then drawn.
		std::vector<std::vector<VisualLine>> itemLines(panel.ItemCount());
		std::vector<float> itemHeights(panel.ItemCount(), 0);

		float total = 0;
		for (size_t i = 0; i < panel.ItemCount(); i++)
		{
			itemLines[i] = BuildItemLines(panel.GetItem(i), textArea.w);
			itemHeights[i] = LinesHeight(itemLines[i]);
			total += itemHeights[i];
		}

		PanelState& state = panelStates[key];

		// A panel whose queue grows is a log: it follows the newest item. A panel whose content
		// is replaced wholesale (the live panels) keeps showing the beginning of what it has.
		if (panel.ItemCount() > state.lastItemCount && state.lastItemCount > 0)
			state.stickToEnd = true;
		state.lastItemCount = panel.ItemCount();

		if (state.stickToEnd)
			state.scroll = 1e9f;				// clamped just below

		float maxScroll = my_max(0.0f, total - itemArea.h);
		state.scroll = my_max(0.0f, my_min(state.scroll, maxScroll));

		// A panel whose content does not fit gets a scrollbar. It is drawn for the panel with the
		// focus only (see Render), but the geometry is remembered for every panel that has one,
		// so that a mouse event can find the thumb.
		if (total > itemArea.h && itemArea.h > 20.0f)
		{
			ScrollGeom geom;
			geom.track = { itemArea.Right() - ScrollbarWidth, itemArea.y, ScrollbarWidth, itemArea.h };
			geom.maxScroll = maxScroll;
			geom.key = key;

			float thumbHeight = my_max(20.0f, itemArea.h * itemArea.h / total);
			float travel = itemArea.h - thumbHeight;
			float thumbY = itemArea.y + ((maxScroll > 0) ? (state.scroll / maxScroll) * travel : 0);

			geom.thumb = { geom.track.x, thumbY, geom.track.w, thumbHeight };

			scrollGeoms[&panel] = geom;
		}

		std::vector<float> positions(panel.ItemCount(), itemArea.y);

		switch (panel.GetOrder())
		{
			case ItemOrder::TopDown:
			{
				float y = itemArea.y - state.scroll;
				for (size_t i = 0; i < panel.ItemCount(); i++)
				{
					positions[i] = y;
					y += itemHeights[i];
				}
				break;
			}

			case ItemOrder::BottomUp:
			{
				float y = itemArea.Bottom() + state.scroll;
				for (size_t i = 0; i < panel.ItemCount(); i++)
				{
					y -= itemHeights[i];
					positions[i] = y;
				}
				break;
			}

			case ItemOrder::LeftRight:
			{
				float x = itemArea.x - state.scroll;
				for (size_t i = 0; i < panel.ItemCount(); i++)
				{
					float width = 0;
					for (size_t l = 0; l < itemLines[i].size(); l++)
					{
						float lw = 0;
						for (size_t r = 0; r < itemLines[i][l].runs.size(); r++)
							lw += MeasureText(itemLines[i][l].runs[r].text);
						width = my_max(width, lw);
					}
					positions[i] = x;
					x += width + 16;
				}
				break;
			}

			case ItemOrder::RightLeft:
			{
				float x = itemArea.Right() + state.scroll;
				for (size_t i = 0; i < panel.ItemCount(); i++)
				{
					float width = 0;
					for (size_t l = 0; l < itemLines[i].size(); l++)
					{
						float lw = 0;
						for (size_t r = 0; r < itemLines[i][l].runs.size(); r++)
							lw += MeasureText(itemLines[i][l].runs[r].text);
						width = my_max(width, lw);
					}
					x -= width;
					positions[i] = x;
					x -= 16;
				}
				break;
			}
		}

		// Everything that was emitted so far (the frames and the titles) is drawn before the
		// scissor box of this panel is set, so that it is not clipped by it.
		FlushBatch();

		glEnable(GL_SCISSOR_TEST);
		glScissor((GLint)itemArea.x, (GLint)(viewportHeight - itemArea.Bottom()),
			(GLsizei)itemArea.w, (GLsizei)itemArea.h);

		for (size_t i = 0; i < panel.ItemCount(); i++)
		{
			if (panel.GetOrder() == ItemOrder::LeftRight || panel.GetOrder() == ItemOrder::RightLeft)
			{
				Rect itemRect = { positions[i], itemArea.y, textArea.w, itemArea.h };
				DrawLines(itemLines[i], itemRect, itemArea.y, panel.GetItem(i).align);
			}
			else
			{
				DrawLines(itemLines[i], textArea, positions[i], panel.GetItem(i).align);
			}
		}

		FlushBatch();
		glDisable(GL_SCISSOR_TEST);

		if (hasCmdline)
		{
			FillRect(cmdArea, ColCmdBg);
			FrameRect(cmdArea, ColPanelBorder, 1.0f);

			float textY = cmdArea.y + 3;
			std::string prompt = "> ";
			DrawText(cmdArea.x + 4, textY, prompt, ColCmdHint);

			float textX = cmdArea.x + 4 + MeasureText(prompt);

			if (cmdline.empty())
			{
				DrawText(textX, textY, "type a JDI command (help lists them)", ColCmdHint);
			}
			else
			{
				DrawText(textX, textY, cmdline, ColCmdText);
			}

			// The caret blinks, so that it is obvious where the typing goes.
			if ((SDL_GetTicks() / 500) % 2 == 0)
			{
				float caretX = textX + MeasureText(cmdline.substr(0, cursor));
				FillRect({ caretX, textY, 1.0f, lineHeight }, ColCmdText);
			}
		}
	}

	// The body of a panel that holds tabs: a strip of the sub-panel titles on top, and the
	// content of the active sub-panel underneath. The sub-panels are ordinary panels, they just
	// do not draw a header of their own - their title is the tab.
	void GlUi::LayoutTabs(const Panel& panel, const Rect& content, const std::string& key)
	{
		size_t count = panel.SubCount();
		if (count == 0)
			return;

		PanelState& state = panelStates[key];
		if (state.activeTab >= count)
			state.activeTab = 0;

		Rect strip = { content.x, content.y, content.w, lineHeight + 6 };
		FillRect(strip, ColPanelTitleBg);

		float x = strip.x;

		for (size_t i = 0; i < count; i++)
		{
			const Panel& sub = panel.Sub(i);

			float width = MeasureText(sub.Title()) + TabPadding * 2;
			if ((x + width) > strip.Right())
				break;

			Rect tab = { x, strip.y, width, strip.h };
			bool active = (i == state.activeTab);

			if (active)
			{
				FillRect(tab, ColPanelBg);
				FillRect({ tab.x, tab.y, tab.w, 2.0f }, ColPanelFocus);
			}
			else if (tab.Contains(mouseX, mouseY))
			{
				FillRect(tab, ColTabHover);
			}

			DrawText(tab.x + TabPadding, tab.y + 3, sub.Title(), active ? ColStrong : ColPanelTitle);

			TabHit hit;
			hit.rect = tab;
			hit.key = key;
			hit.index = i;
			tabHits.push_back(hit);

			x += width;
		}

		// The info of the active tab goes to the right end of the strip: the sub-panel has no
		// header of its own to put it in.
		const Panel& active = panel.Sub(state.activeTab);
		if (!active.Info().empty())
		{
			float infoX = strip.Right() - 4 - MeasureText(active.Info());
			if (infoX > x)
				DrawText(infoX, strip.y + 3, active.Info(), ColPanelTitle);
		}

		Rect body = { content.x, strip.Bottom() + 1, content.w, content.Bottom() - strip.Bottom() - 1 };
		if (body.w < 6 || body.h < 6)
			return;

		LayoutPanel(active, body, key + "/" + std::to_string(state.activeTab), false);
	}


	// ----------------------------------------------------------------------------------------
	// The input
	// ----------------------------------------------------------------------------------------

	void GlUi::InsertText(const char* utf8)
	{
		// The JDI talks ASCII for now, so anything above it is dropped rather than stored in a
		// form the command line could not send back.
		for (size_t i = 0; utf8[i] != 0; i++)
		{
			unsigned char c = (unsigned char)utf8[i];
			if (c < 32 || c > 126)
				continue;

			if (cmdline.size() >= 512)
				break;

			cmdline.insert(cmdline.begin() + cursor, (char)c);
			cursor++;
		}
	}

	void GlUi::HandleSdlKey(const SDL_KeyboardEvent& key)
	{
		if (key.type != SDL_KEYDOWN)
			return;

		switch (key.keysym.scancode)
		{
			case SDL_SCANCODE_RETURN:
			case SDL_SCANCODE_KP_ENTER:
				if (!cmdline.empty() && sink != nullptr)
				{
					sink->OnUiCommand(cmdline);
					cmdline.clear();
					cursor = 0;
					historyPos = -1;
				}
				break;

			case SDL_SCANCODE_BACKSPACE:
				if (cursor > 0)
				{
					cmdline.erase(cmdline.begin() + (cursor - 1));
					cursor--;
				}
				break;

			case SDL_SCANCODE_DELETE:
				if (cursor < cmdline.size())
					cmdline.erase(cmdline.begin() + cursor);
				break;

			case SDL_SCANCODE_LEFT:
				if (cursor > 0)
					cursor--;
				break;

			case SDL_SCANCODE_RIGHT:
				if (cursor < cmdline.size())
					cursor++;
				break;

			case SDL_SCANCODE_HOME:
				cursor = 0;
				break;

			case SDL_SCANCODE_END:
				cursor = cmdline.size();
				break;

			case SDL_SCANCODE_ESCAPE:
				cmdline.clear();
				cursor = 0;
				historyPos = -1;
				break;

			case SDL_SCANCODE_UP:
				if (!cmdHistory.empty())
				{
					if (historyPos < 0)
						historyPos = (int)cmdHistory.size();
					if (historyPos > 0)
						historyPos--;
					cmdline = cmdHistory[historyPos];
					cursor = cmdline.size();
				}
				break;

			case SDL_SCANCODE_DOWN:
				if (historyPos >= 0 && historyPos < (int)cmdHistory.size())
				{
					historyPos++;
					if (historyPos >= (int)cmdHistory.size())
					{
						historyPos = -1;
						cmdline.clear();
					}
					else
					{
						cmdline = cmdHistory[historyPos];
					}
					cursor = cmdline.size();
				}
				break;

			default:
				break;
		}
	}

	void GlUi::MouseDown(float x, float y)
	{
		// A tab takes the click first: it is the only thing in a header that reacts to one.
		for (size_t i = 0; i < tabHits.size(); i++)
		{
			if (!tabHits[i].rect.Contains(x, y))
				continue;

			PanelState& state = panelStates[tabHits[i].key];
			state.activeTab = tabHits[i].index;
			return;
		}

		// Grab the thumb of the scrollbar of the panel with the focus.
		if (hoveredPanel == nullptr)
			return;

		auto it = scrollGeoms.find(hoveredPanel);
		if (it == scrollGeoms.end() || !it->second.thumb.Contains(x, y))
			return;

		dragPanel = hoveredPanel;
		dragOffset = y - it->second.thumb.y;
	}

	void GlUi::MouseUp()
	{
		dragPanel = nullptr;
	}

	void GlUi::DragScrollbar(float y)
	{
		auto it = scrollGeoms.find(dragPanel);
		if (it == scrollGeoms.end())
		{
			// The panel is gone (the debugger published another snapshot): drop the drag.
			dragPanel = nullptr;
			return;
		}

		const ScrollGeom& geom = it->second;
		float travel = geom.track.h - geom.thumb.h;

		if (travel <= 0 || geom.maxScroll <= 0)
			return;

		float thumbY = my_min(my_max(y - dragOffset, geom.track.y), geom.track.y + travel);
		float fraction = (thumbY - geom.track.y) / travel;

		PanelState& state = panelStates[geom.key];
		state.scroll = fraction * geom.maxScroll;
		state.stickToEnd = false;
	}

	void GlUi::HandleSdlEvent(const SDL_Event& event)
	{
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_CLOSE && sink != nullptr)
					sink->OnUiClose();
				break;

			case SDL_TEXTINPUT:
				InsertText(event.text.text);
				break;

			case SDL_KEYDOWN:
				HandleSdlKey(event.key);
				break;

			case SDL_MOUSEMOTION:
				mouseX = (float)event.motion.x;
				mouseY = (float)event.motion.y;
				if (dragPanel != nullptr)
					DragScrollbar(mouseY);
				else
					UpdateHover();
				break;

			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button != SDL_BUTTON_LEFT)
					break;
				mouseX = (float)event.button.x;
				mouseY = (float)event.button.y;
				UpdateHover();
				MouseDown(mouseX, mouseY);
				break;

			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT)
					MouseUp();
				break;

			case SDL_MOUSEWHEEL:
			{
				// Scroll the panel with the focus. The rectangles are the ones of the frame that
				// has just been drawn, which is close enough for a wheel notch.
				if (hoveredPanel != nullptr)
				{
					auto key = panelKeys.find(hoveredPanel);
					if (key != panelKeys.end())
					{
						PanelState& state = panelStates[key->second];
						state.scroll -= event.wheel.y * lineHeight * 3;
						state.stickToEnd = false;
						if (state.scroll < 0)
							state.scroll = 0;
					}
				}
				break;
			}

			default:
				break;
		}
	}

	bool GlUi::HandleEvent(const SDL_Event& event)
	{
		if (!open)
			return false;

		Uint32 id = 0;

		switch (event.type)
		{
			case SDL_WINDOWEVENT:		id = event.window.windowID; break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:				id = event.key.windowID; break;
			case SDL_TEXTINPUT:			id = event.text.windowID; break;
			case SDL_MOUSEMOTION:		id = event.motion.windowID; break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:		id = event.button.windowID; break;
			case SDL_MOUSEWHEEL:		id = event.wheel.windowID; break;
			default:					return false;
		}

		if (id != windowId)
			return false;

		HandleSdlEvent(event);
		return true;
	}


	// ----------------------------------------------------------------------------------------
	// The window
	// ----------------------------------------------------------------------------------------

	bool GlUi::Open(const std::string& title, Sink* sink)
	{
		this->sink = sink;
		sessionPath = title;

		if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
		{
			if (SDL_Init(SDL_INIT_VIDEO) != 0)
			{
				Debug::Report(Debug::Channel::Error, "debugui2: SDL video: %s\n", SDL_GetError());
				return false;
			}
			sdlOwned = true;
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

		window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			1400, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

		if (window == nullptr)
		{
			Debug::Report(Debug::Channel::Error, "debugui2: SDL_CreateWindow: %s\n", SDL_GetError());
			Close();
			return false;
		}

		context = SDL_GL_CreateContext(window);
		if (context == nullptr)
		{
			Debug::Report(Debug::Channel::Error, "debugui2: SDL_GL_CreateContext: %s\n", SDL_GetError());
			Close();
			return false;
		}

		windowId = SDL_GetWindowID(window);

		// The context has to be current for GLEW and for the atlas. The context of the emulator's
		// renderer belongs to its own thread and is not touched from here.
		SDL_GL_MakeCurrent(window, context);

		glewExperimental = GL_TRUE;
		GLenum err = glewInit();
		if (err != GLEW_OK)
		{
			Debug::Report(Debug::Channel::Error, "debugui2: glewInit: %s\n", glewGetErrorString(err));
			Close();
			return false;
		}

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		GLuint vs = CompileShader(GL_VERTEX_SHADER, VertexShaderSource);
		GLuint fs = CompileShader(GL_FRAGMENT_SHADER, FragmentShaderSource);

		if (vs == 0 || fs == 0)
		{
			if (vs) glDeleteShader(vs);
			if (fs) glDeleteShader(fs);
			Close();
			return false;
		}

		program = glCreateProgram();
		glAttachShader(program, vs);
		glAttachShader(program, fs);
		glLinkProgram(program);
		glDeleteShader(vs);
		glDeleteShader(fs);

		GLint linked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (!linked)
		{
			char log[0x400] = { 0, };
			glGetProgramInfoLog(program, sizeof(log) - 1, nullptr, log);
			Debug::Report(Debug::Channel::Error, "debugui2: link: %s\n", log);
			Close();
			return false;
		}

		viewportUniform = glGetUniformLocation(program, "uViewport");
		glUseProgram(program);
		glUniform1i(glGetUniformLocation(program, "uTexture"), 0);

		if (!BuildFont())
		{
			Close();
			return false;
		}

		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, mode));
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, r));

		glBindVertexArray(0);

		SDL_StartTextInput();

		open = true;
		Debug::Report(Debug::Channel::Norm, "debugui2: session window opened (%s)\n", title.c_str());
		return true;
	}

	void GlUi::Close()
	{
		if (window != nullptr || context != nullptr)
		{
			SDL_GL_MakeCurrent(window, context);

			for (auto it = images.begin(); it != images.end(); ++it)
			{
				if (it->second.texture != 0)
					glDeleteTextures(1, &it->second.texture);
			}
			images.clear();

			if (vbo) glDeleteBuffers(1, &vbo);
			if (vao) glDeleteVertexArrays(1, &vao);
			if (atlas) glDeleteTextures(1, &atlas);
			if (program) glDeleteProgram(program);

			vbo = vao = atlas = program = 0;

			SDL_GL_MakeCurrent(nullptr, nullptr);
		}

		if (context != nullptr)
		{
			SDL_GL_DeleteContext(context);
			context = nullptr;
		}

		if (window != nullptr)
		{
			SDL_DestroyWindow(window);
			window = nullptr;
		}

		if (sdlOwned)
		{
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			sdlOwned = false;
		}

		open = false;
		windowId = 0;
		sink = nullptr;
	}

	void GlUi::Render(const std::shared_ptr<const View>& view)
	{
		if (!open || window == nullptr || view == nullptr)
			return;

		// The emulator draws through its own context on its own thread; this one is made current
		// only for the duration of the frame, and then whatever was current is put back.
		SDL_Window* previousWindow = SDL_GL_GetCurrentWindow();
		SDL_GLContext previousContext = SDL_GL_GetCurrentContext();

		SDL_GL_MakeCurrent(window, context);

		cmdHistory = view->cmdHistory;

		// The snapshot is kept until the next frame, so that the mouse events in between can walk
		// the tree that is on the screen.
		renderedView = view;

		int width = 0, height = 0;
		SDL_GetWindowSize(window, &width, &height);

		if (width > 0 && height > 0)
		{
			glViewport(0, 0, width, height);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glDisable(GL_SCISSOR_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			uint8_t r, g, b, a;
			UnpackColor(ColWindowBg, &r, &g, &b, &a);
			glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			panelRects.clear();
			panelKeys.clear();
			scrollGeoms.clear();
			tabHits.clear();

			BeginBatch(width, height);
			FillRect({ 0, 0, (float)width, (float)height }, ColWindowBg);
			FlushBatch();

			LayoutPanel(view->root, { 4, 4, (float)width - 8, (float)height - 8 }, "");

			UpdateHover();

			// The panel with the focus is marked and, when its content does not fit, gets the
			// scrollbar. Both are drawn after the rest, so that they go over the frames of the
			// panels around it.
			if (hoveredPanel != nullptr)
			{
				auto rect = panelRects.find(hoveredPanel);
				if (rect != panelRects.end())
					FrameRect(rect->second, ColPanelFocus, 2.0f);

				auto bar = scrollGeoms.find(hoveredPanel);
				if (bar != scrollGeoms.end())
					DrawScrollbar(bar->second);
			}

			FlushBatch();
			SDL_GL_SwapWindow(window);
		}

		SDL_GL_MakeCurrent(previousWindow, previousContext);
	}


	// ----------------------------------------------------------------------------------------
	// The module entry points
	// ----------------------------------------------------------------------------------------

	Ui* CreateGlUi()
	{
		GlUi* ui = new GlUi();
		g_GlUi = ui;
		return ui;
	}

	bool UiSdlEvent(const SDL_Event& event)
	{
		if (g_GlUi == nullptr)
			return false;

		return g_GlUi->HandleEvent(event);
	}

	void UiPumpSdlEvents()
	{
		if (g_GlUi == nullptr || !g_GlUi->IsOpen())
			return;

		// A host without an event loop of its own (the Win32 front end) leaves the pumping to the
		// debugger: the events that are not ours are put back for the host to deal with.
		std::vector<SDL_Event> foreign;
		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			if (!g_GlUi->HandleEvent(event))
				foreign.push_back(event);
		}

		for (size_t i = 0; i < foreign.size(); i++)
			SDL_PushEvent(&foreign[i]);
	}

}
