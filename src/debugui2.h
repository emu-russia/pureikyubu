/*

# DebugUI2

The new debugger (codenamed `debugui2`). It is meant to replace the legacy debug console
(debugui.cpp / cui.cpp), which is kept around for reference until the new one is complete.

Unlike the legacy console, this debugger is not a wall of little windows with buttons and knobs.
The whole point is minimalism: the user types a command, the debugger answers with Markdown.

## How it is put together

- The debugger works in its own thread (as before, non-invasively). The emulated system does not
  know that it is being debugged: everything the debugger does goes through JDI and the Debug API.
- The user interface lives in a separate window and is drawn with OpenGL. Drawing the window is
  the only part that has to happen on the UI thread (a GL context belongs to the thread that
  created it, and SDL wants its events pumped from one place), so the window is driven by the host
  front end: it calls `Debug2::Frame` once per frame and feeds the events it pumps through
  `Debug2::UiSdlEvent`. Everything else - the session, the command line, the collecting of debug
  messages and the building of the panels - is done by the debugger thread.
- The way the UI is presented is behind the abstract `Debug2::Ui` class. The reference
  implementation is the GL one (`debugui2gl.cpp`); nothing in the core knows about OpenGL, about
  SDL or about the front end that happens to be hosting us.

## The view

The UI is a space filled with panels. A panel is either a leaf that holds a queue of items, or it
is split vertically (into side-by-side columns) or horizontally (into stacked rows) into sub-panels.
A panel can also carry the "has a command line" flag - in the mockup only the message history panel
has one.

An item is a MarkdownItem: a piece of source Markdown. `MarkdownToView` forms the view (a list of
styled lines) from that source, and the front end wraps and draws those lines. Items are queued in
the panel and can be laid out top-down, bottom-up, left-to-right or right-to-left, and each item is
aligned to the left or to the right edge of its panel. The debug messages are aligned to the left,
the echo of the commands the user typed - to the right.

Markdown is the main format for structured output: a JDI command that has something to show returns
an object with a `markdown` member and the debugger puts it into a panel as a new item. The
artifacts a command creates (binaries, PNGs) go next to the session, and image references in the
Markdown are resolved against the session folder.

## Text on GL

The GL front end does not use a UI toolkit. It rasterizes the glyphs it needs out of a TrueType
font with `stb_truetype`, packs them into one texture atlas and draws the text with its own
shader, one batched quad per glyph.

The font is a Data file (`Data/DebugUiMono.ttf`, DejaVu Sans Mono - the Bitstream Vera licence it
comes under travels with it as `Data/DebugUiMono.LICENSE`), so that it can be replaced without
rebuilding. Right now the JDI input and output is ASCII (UTF-8 is a separate task), but the atlas
is not limited to ASCII: it packs the ranges the renderer is likely to be asked for (Latin, Greek,
Cyrillic, box drawing, arrows, mathematical operators and the common symbols), so the text side is
ready for the day the interface switches to UTF-8.

## Sessions

The debugger works within a session: `Data/Sessions/<name>_<ordinal>`, where the name is taken
from the loaded image (`pong.dol` gives `pong`) with the characters that do not belong in a file
name replaced, and the ordinal makes the folder unique. The session is a JDI entity: its path is
available to the handlers through the `SessionPath` command, and `SessionSave` stores a text
artifact in it. Closing the debugger closes the session - the collected log is written into the
session folder as `log.md`.

*/

#pragma once

#include <memory>
#include <deque>

namespace Debug2
{

	// ------------------------------------------------------------------------------------
	// Markdown -> view
	// ------------------------------------------------------------------------------------

	// The style of a run of text within a line.
	enum class MdStyle
	{
		Norm = 0,
		Emphasis,			// *italic*
		Strong,				// **bold**
		Code,				// `inline` or a line inside a fenced block
		Heading,			// # .. ###### (the level is in MdSpan::level)
		Bullet,				// a list item
		Rule,				// a horizontal rule (the text is empty)
		Image,				// ![alt](file) - the file is relative to the session folder
	};

	// A run of text with one style.
	struct MdSpan
	{
		MdStyle style = MdStyle::Norm;
		int level = 0;				// heading level, 1..6
		std::string text;			// the text itself, the markup is already stripped
		std::string ref;			// the image file name (MdStyle::Image)
	};

	// One line of the view. A line is a sequence of styled runs.
	struct MdLine
	{
		std::vector<MdSpan> spans;
	};

	// The view formed from a source Markdown item.
	using MdView = std::vector<MdLine>;

	// Form the view from the source Markdown text (the "Markdown -> view" module).
	void MarkdownToView(const std::string& markdown, MdView& view);

	// Where an item is aligned inside its panel.
	enum class ItemAlign
	{
		Left = 0,
		Right,
	};

	// A source Markdown item, together with the view formed from it.
	struct Item
	{
		std::string source;
		MdView view;
		ItemAlign align = ItemAlign::Left;
	};

	// The order in which the queued items are placed in the panel.
	enum class ItemOrder
	{
		TopDown = 0,
		BottomUp,
		LeftRight,
		RightLeft,
	};

	// A vertical split puts the sub-panels side by side (columns), a horizontal one stacks them
	// (rows). `None` means the panel is a leaf and holds items.
	enum class Split
	{
		None = 0,
		Vertical,
		Horizontal,
	};

	class Panel
	{
		std::string title;
		Split split = Split::None;
		std::vector<Panel*> subs;
		std::deque<Item> items;
		bool cmdline = false;
		ItemOrder order = ItemOrder::TopDown;

	public:
		Panel() = default;
		explicit Panel(const std::string& title) : title(title) {}
		Panel(const Panel& other) { *this = other; }
		Panel& operator=(const Panel& other);
		~Panel();

		const std::string& Title() const { return title; }
		void SetTitle(const std::string& value) { title = value; }

		Split GetSplit() const { return split; }

		// Turn the panel into `count` empty sub-panels. The panel keeps its title and the
		// command line flag; the items it had are dropped (a split panel is not a leaf anymore).
		void SplitInto(Split direction, size_t count);
		size_t SubCount() const { return subs.size(); }
		Panel& Sub(size_t n) { return *subs[n]; }
		const Panel& Sub(size_t n) const { return *subs[n]; }

		void AddItem(const Item& item) { items.push_back(item); }
		void AddItem(Item&& item) { items.push_back(std::move(item)); }
		void DropFirstItem() { if (!items.empty()) items.pop_front(); }
		void ClearItems() { items.clear(); }
		size_t ItemCount() const { return items.size(); }
		const Item& GetItem(size_t n) const { return items[n]; }

		bool HasCmdline() const { return cmdline; }
		void SetCmdline(bool value) { cmdline = value; }

		ItemOrder GetOrder() const { return order; }
		void SetOrder(ItemOrder value) { order = value; }
	};

	// A complete snapshot of what the front end is to draw. The debugger thread publishes a new
	// snapshot whenever the panel tree changed, so the UI thread can render from it without
	// holding a lock (and without the debugger thread ever waiting for the renderer).
	struct View
	{
		std::string title;					// the session path (goes into the window title)
		Panel root;
		std::vector<std::string> cmdHistory;	// the command line history, the oldest first
	};


	// ------------------------------------------------------------------------------------
	// The front end
	// ------------------------------------------------------------------------------------

	// How the debugger presents itself. The core only knows this interface: it builds the view
	// and hands it over. A different presentation (a terminal, a remote client) only has to
	// implement this class, which is what makes the GL implementation the "typical" one rather
	// than the only possible one.
	class Ui
	{
	public:
		// Any front end delivers the user's actions through this interface.
		class Sink
		{
		public:
			virtual ~Sink() = default;

			// The user finished typing a command.
			virtual void OnUiCommand(const std::string& cmdline) = 0;

			// The user closed the debugger window.
			virtual void OnUiClose() = 0;
		};

		virtual ~Ui() = default;

		virtual bool Open(const std::string& title, Sink* sink) = 0;
		virtual void Close() = 0;
		virtual bool IsOpen() const = 0;

		// Draw one frame. Called from the thread that owns the window.
		virtual void Render(const View& view) = 0;
	};

	// The reference front end: an SDL window with an OpenGL 3.3 context and a glyph atlas built
	// at start-up. Returns nullptr when the platform cannot provide a window (no SDL video, no
	// usable GL context); the debugger then keeps working without a window.
	Ui* CreateGlUi();


	// ------------------------------------------------------------------------------------
	// The debugger
	// ------------------------------------------------------------------------------------

	class Debugger : public Ui::Sink
	{
		// ---- only touched by the debugger thread ----

		Thread* thread = nullptr;
		Panel* log = nullptr;
		Panel* regs = nullptr;
		Panel* disasm = nullptr;
		Panel* memdump = nullptr;

		std::string sessionPath;
		std::string sessionName;

		std::vector<std::string> cmdHistory;
		size_t cmdHistoryPos = 0;

		uint64_t lastRefresh = 0;

		// ---- shared with the UI thread ----

		SpinLock lock;
		bool active = false;
		bool dirty = false;
		Panel root;
		std::shared_ptr<const View> published;

		std::deque<std::string> pendingCommands;	// from the UI thread to the debugger thread
		bool closeRequested = false;

		Ui* ui = nullptr;

		void ThreadProc();
		static void ThreadEntry(void* param);

		void ExecuteCommand(const std::string& cmdline);
		void PumpMessages();
		void RefreshLivePanels();
		void Publish();

		void AppendItem(Panel* panel, const std::string& markdown, ItemAlign align);
		void ReplaceItems(Panel* panel, const std::string& markdown);

		// Ask JDI and turn the answer into the source of an item. An empty result means the
		// command had nothing to say (the handlers report their problems through Debug::Report).
		static std::string JdiCommandToMarkdown(const std::string& cmdline);

		bool CreateSession();

	public:
		Debugger();
		virtual ~Debugger();

		// Create the session, the panel tree and the window, then start the debugger thread.
		bool Start();

		// Stop the thread, close the window and close (save) the session.
		void Stop();

		bool IsActive() const { return active; }

		// Render one frame. The host UI thread calls this from its frame loop.
		void Frame();

		// The session is a JDI entity.
		const std::string& SessionPath() const { return sessionPath; }

		// Store a text artifact in the session folder and return its full path ("" on failure).
		std::string SessionSave(const std::string& filename, const std::string& text) const;

		// Ui::Sink
		virtual void OnUiCommand(const std::string& cmdline);
		virtual void OnUiClose();
	};

	// The running debugger, or nullptr when it is not open.
	extern Debugger* g_Debugger;

	// Register the commands of the session (the JDI node is registered by the host UI along
	// with the other nodes; see JdiSpecs::DebugUi2Jdi).
	void Reflector();

	// The front end entry points (see the Debug menu of the host UI).
	void StartDebugger();
	void StopDebugger();
	bool IsDebuggerActive();

	// True once the user closed the debugger window. The host checks this from its frame loop
	// and calls StopDebugger() - the window cannot tear the debugger down from inside its own
	// event callback.
	bool CloseRequested();

	// Render one frame (a no-op when the debugger is not open).
	void Frame();

	// Feed one SDL event. Returns true when the event belonged to the debugger window, in which
	// case the host must not pass it to its own UI.
	bool UiSdlEvent(const SDL_Event& event);

	// A host that has no event loop of its own (the Win32 front end) calls this once a frame: the
	// debugger pumps SDL itself and re-queues the events that are not its own.
	void UiPumpSdlEvents();

}
