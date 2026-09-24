// The SDL2 frontend of the GBA and Game Boy emulators (see gba_sdl.h).
//
// The portable machines carry the new debugger (debugui2) with them. This file cannot talk to it
// directly - the debugger's headers are part of the GameCube side, which needs the emulator's own
// precompiled header, and this module deliberately does not pull that in (see src/gba/Readme.md).
// The bridge in gba_debug.cpp is the host-facing half of it instead: this frontend starts and
// stops the debugger session through `DebugStart` / `DebugStop` and drives it from its event loop
// with `DebugPumpEvents` / `DebugFrame`.

#include "gba_sdl.h"
#include "gba.h"
#include "gba_audio.h"
#include "gba_bootrom.h"
#include "gba_debug.h"
#include "gb.h"

#define SDL_MAIN_HANDLED

#ifdef _WIN32
#include "SDL.h"
#else
#include <SDL2/SDL.h>
#endif

#include <cctype>
#include <cstring>
#include <vector>

namespace GBA
{
	/// <summary>
	/// What the frontend's own keys asked for. The two toggles (fast forward, full screen) are the
	/// state itself and live here across frames; the rest are requests for the iteration that saw
	/// the key and are cleared with the struct.
	/// </summary>
	struct Hotkeys
	{
		bool fastForward = false;		// F1 toggles
		bool fullscreen = false;		// F11 toggles
		bool debugger = false;			// F2
		bool screenshot = false;		// F12
		bool saveBattery = false;		// F5: write the cartridge's .sav
		bool saveState = false;			// F3: write the quick save of the current slot
		bool loadState = false;			// F4: read it back
		int slotStep = 0;				// Shift+F3 (-1) / Shift+F4 (+1): pick another slot
	};

	// ---------------------------------------------------------------------------------------
	// Small helpers
	// ---------------------------------------------------------------------------------------

	static bool HasExtension(const std::string& path, const char* extension)
	{
		size_t dot = path.find_last_of('.');
		if (dot == std::string::npos)
		{
			return false;
		}

		std::string ext = path.substr(dot);

		for (char& c : ext)
		{
			c = (char)tolower((unsigned char)c);
		}

		return ext == extension;
	}

	bool IsGameBoyImage(const std::string& path)
	{
		// .gba/.agb = Game Boy Advance, .gb/.gbc/.sgb = the Game Boy family. A ROM with an
		// ambiguous extension (.bin, .rom) is run with the explicit `--gba <file>` form.
		return HasExtension(path, ".gba") || HasExtension(path, ".agb")
			|| HasExtension(path, ".gb") || HasExtension(path, ".gbc") || HasExtension(path, ".sgb");
	}

	bool IsDmgImage(const std::string& path)
	{
		return HasExtension(path, ".gb") || HasExtension(path, ".gbc") || HasExtension(path, ".sgb");
	}

	const char* DefaultSettingsPath()
	{
		return "Data/GBASettings.json";
	}

	// The log sink: the cores report through GBA::Log, which the frontend routes to the console.
	static void FrontendLog(LogLevel level, const char* text, void* user)
	{
		static const char* names[] = { "error", "warning", "info", "debug" };
		int index = (int)level;
		if (index < 0 || index > 3)
		{
			index = 2;
		}

		printf("emu: [%s] %s\n", names[index], text);
		fflush(stdout);
	}

	bool LoadSettings(const std::string& path, GbaSettings& settings, std::string& usedPath, std::string* error)
	{
		std::vector<std::string> candidates;

		if (!path.empty())
		{
			candidates.push_back(path);
		}
		else
		{
			candidates.push_back(DefaultSettingsPath());
			candidates.push_back("../build/Data/GBASettings.json");
			candidates.push_back("build/Data/GBASettings.json");
		}

		settings = GbaSettings::Defaults();
		usedPath = candidates.front();

		for (const std::string& candidate : candidates)
		{
			FILE* probe = fopen(candidate.c_str(), "rb");

			if (probe == nullptr)
			{
				continue;
			}

			fclose(probe);

			usedPath = candidate;
			return GbaSettings::Load(candidate, settings, error);
		}

		// No file anywhere: the built-in defaults are used and the caller may write them back.
		if (error != nullptr)
		{
			error->clear();
		}

		return true;
	}

	// ---------------------------------------------------------------------------------------
	// The SDL2 host: the window, the streaming texture and the sound device
	// ---------------------------------------------------------------------------------------

	class Host
	{
	public:
		SDL_Window* window = nullptr;
		SDL_Renderer* renderer = nullptr;
		SDL_Texture* texture = nullptr;
		SDL_AudioDeviceID audio = 0;

		// The mixer buffer the audio callback plays (see gba_audio.h). The machine pushes into
		// it every frame; SDL pulls from it on its own audio thread.
		AudioBuffer sound;

		int width = 0;
		int height = 0;
		int sampleRate = 32768;
		bool vsync = false;
		bool audioEnabled = false;

		// The LCD effect (settings.video.lcdEffect, dmgemu's `lcd_effect`): the frame that is
		// shown is the blend of the machine's frame with the last one shown, which is the
		// ghosting an LCD has while the picture moves. `lcdBuffer` is that last shown frame; it
		// starts black, exactly as dmgemu's frame buffer does, so the first frames fade in.
		bool lcdEffect = true;
		std::vector<uint32_t> lcdBuffer;

		double fps = 0.0;
		int screenshots = 0;

		~Host()
		{
			if (audio != 0) SDL_CloseAudioDevice(audio);
			if (texture != nullptr) SDL_DestroyTexture(texture);
			if (renderer != nullptr) SDL_DestroyRenderer(renderer);
			if (window != nullptr) SDL_DestroyWindow(window);
		}

		bool Open(const char* title, int frameWidth, int frameHeight, const GbaSettings& settings)
		{
			width = frameWidth;
			height = frameHeight;
			sampleRate = settings.sampleRate;
			audioEnabled = settings.audioEnabled;
			lcdEffect = settings.lcdEffect;

			if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
			{
				printf("emu: SDL_Init failed: %s\n", SDL_GetError());
				return false;
			}

			Uint32 flags = SDL_WINDOW_RESIZABLE;

			if (settings.fullscreen)
			{
				flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
			}

			int scale = settings.videoScale > 0 ? settings.videoScale : 1;

			window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
				width * scale, height * scale, flags);

			if (window == nullptr)
			{
				printf("emu: cannot create the window: %s\n", SDL_GetError());
				return false;
			}

			Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;

			vsync = settings.vsync;

			if (vsync)
			{
				rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
			}

			renderer = SDL_CreateRenderer(window, -1, rendererFlags);

			if (renderer == nullptr)
			{
				// A machine without an accelerated renderer still runs the emulator.
				renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
			}

			if (renderer == nullptr)
			{
				printf("emu: cannot create the renderer: %s\n", SDL_GetError());
				return false;
			}

			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, settings.integerScale ? "0" : "1");

			texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_STREAMING, width, height);

			if (texture == nullptr)
			{
				printf("emu: cannot create the texture: %s\n", SDL_GetError());
				return false;
			}

			if (audioEnabled)
			{
				// The device is opened in *callback* mode: the mixer buffer is what feeds it (see
				// gba_audio.h), and SDL calls AudioCallback on its own audio thread with a period
				// of `samples` frames. 512 frames is about 16 ms at the GBA's 32768 Hz: short
				// enough to not add much delay, long enough that the OS is not asked for a
				// miniature buffer.
				SDL_AudioSpec want{}, have{};
				want.freq = sampleRate;
				want.format = AUDIO_S16SYS;
				want.channels = 2;
				want.samples = 512;
				want.callback = AudioCallback;
				want.userdata = &sound;

				audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

				if (audio == 0)
				{
					printf("emu: no audio device (%s); running silently\n", SDL_GetError());
					audioEnabled = false;
				}
				else
				{
					// The core follows the device's rate if it gave us another one. The buffer is
					// sized and primed before the device is started, so its first callbacks
					// already find the cushion of silence.
					sampleRate = have.freq;
					sound.Reset(have.freq, have.samples);
					sound.Prime();
					SDL_PauseAudioDevice(audio, 0);
				}
			}

			return true;
		}

		/// <summary>
		/// Show one frame. With the LCD effect on it is blended half and half with the frame
		/// before it (`(previous &gt;&gt; 1) + (current &gt;&gt; 1)`, the recipe dmgemu's
		/// `lcd_refresh` uses): a still picture settles after a few frames, a moving one leaves
		/// the ghosting a real LCD has. The blend is recursive - the buffer holds the frame that
		/// was shown, not the machine's - so the trail decays by half every frame. Each of the
		/// three colour bytes is halved on its own; the mask drops the bit that a shift pushes
		/// across a byte boundary. The machine's own frame is never touched, so `FrameBuffer()`
		/// - what the screenshots and the debug interface read - stays clean.
		/// </summary>
		void Present(const uint32_t* pixels)
		{
			const uint32_t* shown = pixels;

			if (lcdEffect)
			{
				size_t count = (size_t)width * (size_t)height;

				if (lcdBuffer.size() != count)
				{
					lcdBuffer.assign(count, 0);
				}

				for (size_t i = 0; i < count; i++)
				{
					lcdBuffer[i] = (0x7F7F7Fu & (lcdBuffer[i] >> 1)) + (0x7F7F7Fu & (pixels[i] >> 1));
				}

				shown = lcdBuffer.data();
			}

			SDL_UpdateTexture(texture, nullptr, shown, width * 4);
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, nullptr, nullptr);
			SDL_RenderPresent(renderer);
		}

		/// <summary>
		/// SDL's audio callback (dmgemu's `Mixer`): it plays what the machine pushed into the
		/// mixer buffer. `stream` holds `len` bytes of AUDIO_S16SYS stereo frames, and the buffer
		/// fills the whole of it - the samples it has, then silence.
		/// </summary>
		static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len)
		{
			AudioBuffer* mixer = (AudioBuffer*)userdata;

			if (mixer != nullptr)
			{
				mixer->Play((int16_t*)stream, len / (int)(2 * sizeof(int16_t)));
			}
		}

		/// <summary>True while the mixer buffer is short enough that another frame is needed at
		/// once to refill it (the catch-up of the frame loop).</summary>
		bool SoundStarving() const
		{
			return audio != 0 && audioEnabled && sound.Starving();
		}

		/// <summary>
		/// Steer the mixer's playback rate from the buffer's level (see AudioBuffer::UpdateClock):
		/// this is what lets the machine keep its own speed while the sound device's clock is
		/// slightly different.
		/// </summary>
		void UpdateAudioClock()
		{
			if (audio != 0 && audioEnabled)
			{
				sound.UpdateClock();
			}
		}

		/// <summary>Push the samples the machine mixed in one frame into the mixer buffer.</summary>
		void PushAudio(const int16_t* frames, int count)
		{
			if (audio != 0 && audioEnabled)
			{
				sound.Push(frames, count);
			}
		}

		/// <summary>
		/// The sound status for the window title: the delay the buffer adds, how far the device's
		/// clock is from the machine's (the mixer's rate correction), and the counters that say the
		/// sound is not keeping up.
		/// </summary>
		std::string AudioText() const
		{
			if (audio == 0 || !audioEnabled)
			{
				return std::string();
			}

			char text[128];
			snprintf(text, sizeof(text), " - sound %i ms", sound.LatencyMs());

			std::string status = text;

			int rate = sound.RatePermille();

			if (rate != 1000)
			{
				int delta = rate - 1000;
				snprintf(text, sizeof(text), " (%s%i.%i%%)", delta < 0 ? "-" : "+",
					(delta < 0 ? -delta : delta) / 10, (delta < 0 ? -delta : delta) % 10);
				status += text;
			}

			if (sound.Underruns() > 0 || sound.Drops() > 0)
			{
				snprintf(text, sizeof(text), " [%u gaps, %u drops]",
					sound.Underruns(), sound.Drops());
				status += text;
			}

			return status;
		}

		void HandleHotkey(const SDL_Event& event, Hotkeys& keys)
		{
			if (event.type != SDL_KEYDOWN)
			{
				return;
			}

			// Shift turns the two save state keys into the slot selection (F3/F4 write and read
			// the current slot, Shift+F3/Shift+F4 step through the ten of them).
			bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;

			switch (event.key.keysym.sym)
			{
				case SDLK_F1: keys.fastForward = !keys.fastForward; break;
				case SDLK_F2: keys.debugger = true; break;
				case SDLK_F3: if (shift) keys.slotStep = -1; else keys.saveState = true; break;
				case SDLK_F4: if (shift) keys.slotStep = 1; else keys.loadState = true; break;
				case SDLK_F5: keys.saveBattery = true; break;
				case SDLK_F11: keys.fullscreen = !keys.fullscreen; break;
				case SDLK_F12: keys.screenshot = true; break;
				default: break;
			}
		}

		void ApplyFullscreen(bool fullscreen)
		{
			if (fullscreen != ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0))
			{
				SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
			}
		}

		void UpdateFps()
		{
			static uint32_t timer = 0;
			static int frames = 0;

			uint32_t now = SDL_GetTicks();

			if (timer == 0)
			{
				timer = now;
			}

			frames++;

			if (now - timer >= 1000)
			{
				fps = frames * 1000.0 / (now - timer);
				timer = now;
				frames = 0;
			}
		}

		void SaveScreenshot(const uint32_t* pixels)
		{
			SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
				SDL_PIXELFORMAT_ARGB8888);

			if (surface == nullptr)
			{
				return;
			}

			for (int y = 0; y < height; y++)
			{
				memcpy((uint8_t*)surface->pixels + y * surface->pitch, pixels + (size_t)y * width,
					(size_t)width * 4);
			}

			char name[128];
			snprintf(name, sizeof(name), "emu_screenshot_%03i.bmp", screenshots++);
			SDL_SaveBMP(surface, name);
			SDL_FreeSurface(surface);

			printf("emu: screenshot -> %s\n", name);
		}

		/// <summary>
		/// Wait for the next frame boundary: the machine's own frame rate is the wall clock's
		/// (59.7275 Hz for both machines), whatever the display does. With vsync on the renderer
		/// already waits for the display, and this only takes up the slack between its refresh and
		/// the machine's (so a 120 Hz display does not run the machine twice as fast); with vsync
		/// off it is the only thing pacing the loop.
		/// </summary>
		void PaceFrame(uint64_t frameIndex, uint32_t startTicks, double frameMilliseconds, uint32_t& paceStart, uint64_t& paceFrames)
		{
			paceFrames++;
			uint32_t next = paceStart + (uint32_t)(frameMilliseconds * paceFrames);
			uint32_t now = SDL_GetTicks();

			if (now < next)
			{
				SDL_Delay(next - now);
			}
			else if (now > next + 250)
			{
				// The host could not keep up: rebase the schedule instead of spinning.
				paceStart = now;
				paceFrames = 0;
			}

			(void)frameIndex;
			(void)startTicks;
		}
	};

	// ---------------------------------------------------------------------------------------
	// Input
	// ---------------------------------------------------------------------------------------

	/// <summary>The key state of the GBA, mapped from the settings' bindings.</summary>
	class GbaInput
	{
		const GbaSettings& settings;
		uint16_t pressed = 0;
		SDL_GameController* controller = nullptr;

	public:
		explicit GbaInput(const GbaSettings& settings) : settings(settings)
		{
			if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0))
			{
				controller = SDL_GameControllerOpen(0);
			}
		}

		~GbaInput()
		{
			if (controller != nullptr)
			{
				SDL_GameControllerClose(controller);
			}
		}

		uint16_t Pressed() const { return pressed; }

		/// <summary>Feed one SDL event to the key state. Answers false when the user asked to
		/// quit (the window's close button or Escape).</summary>
		bool Handle(const SDL_Event& event)
		{
			switch (event.type)
			{
				case SDL_QUIT:
					return false;

				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					bool down = (event.type == SDL_KEYDOWN);

					if (event.key.keysym.sym == SDLK_ESCAPE && down)
					{
						return false;
					}

					// A host key that is bound to a GBA key toggles that key's bit. The binding
					// names are the ones SDL itself uses ("X", "Return", "Up", "Space"...).
					uint16_t bit = settings.KeyBitFor(SDL_GetKeyName(event.key.keysym.sym));

					if (bit != 0)
					{
						if (down) pressed |= bit; else pressed &= (uint16_t)~bit;
					}
					break;
				}

				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP:
				{
					bool down = (event.type == SDL_CONTROLLERBUTTONDOWN);
					uint16_t bit = ControllerBit((SDL_GameControllerButton)event.cbutton.button);

					if (bit != 0)
					{
						if (down) pressed |= bit; else pressed &= (uint16_t)~bit;
					}
					break;
				}

				default:
					break;
			}

			return true;
		}

	private:
		/// <summary>The fixed game controller layout: A/B, Start/Back, the d-pad and the shoulders.</summary>
		static uint16_t ControllerBit(SDL_GameControllerButton button)
		{
			switch (button)
			{
				case SDL_CONTROLLER_BUTTON_A: return KEY_A;
				case SDL_CONTROLLER_BUTTON_B: return KEY_B;
				case SDL_CONTROLLER_BUTTON_X: return KEY_B;
				case SDL_CONTROLLER_BUTTON_Y: return KEY_A;
				case SDL_CONTROLLER_BUTTON_START: return KEY_START;
				case SDL_CONTROLLER_BUTTON_BACK: return KEY_SELECT;
				case SDL_CONTROLLER_BUTTON_DPAD_UP: return KEY_UP;
				case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return KEY_DOWN;
				case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return KEY_LEFT;
				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return KEY_RIGHT;
				case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return KEY_L;
				case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return KEY_R;
				default: return 0;
			}
		}
	};

	/// <summary>
	/// The key state of the Game Boy. The Game Boy has eight buttons and no bindings section of its
	/// own, so the layout is fixed (and documented in wiki/gba.md): the arrow keys, Z = A,
	/// X = B, Return = Start, Backspace = Select, and the controller's A/B/Start/Back/d-pad.
	/// </summary>
	class GbInput
	{
		uint8_t pressed = 0;
		SDL_GameController* controller = nullptr;

	public:
		GbInput()
		{
			if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0))
			{
				controller = SDL_GameControllerOpen(0);
			}
		}

		~GbInput()
		{
			if (controller != nullptr)
			{
				SDL_GameControllerClose(controller);
			}
		}

		uint8_t Pressed() const { return pressed; }

		/// <summary>Feed one SDL event to the key state. Answers false when the user asked to
		/// quit (the window's close button or Escape).</summary>
		bool Handle(const SDL_Event& event)
		{
			switch (event.type)
			{
				case SDL_QUIT:
					return false;

				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					bool down = (event.type == SDL_KEYDOWN);

					if (event.key.keysym.sym == SDLK_ESCAPE && down)
					{
						return false;
					}

					uint8_t bit = KeyBit(event.key.keysym.sym);

					if (bit != 0)
					{
						if (down) pressed |= bit; else pressed &= (uint8_t)~bit;
					}
					break;
				}

				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP:
				{
					bool down = (event.type == SDL_CONTROLLERBUTTONDOWN);
					uint8_t bit = ControllerBit((SDL_GameControllerButton)event.cbutton.button);

					if (bit != 0)
					{
						if (down) pressed |= bit; else pressed &= (uint8_t)~bit;
					}
					break;
				}

				default:
					break;
			}

			return true;
		}

	private:
		static uint8_t KeyBit(SDL_Keycode key)
		{
			switch (key)
			{
				case SDLK_z: return GbButtonA;
				case SDLK_x: return GbButtonB;
				case SDLK_RETURN: return GbButtonStart;
				case SDLK_BACKSPACE: return GbButtonSelect;
				case SDLK_RIGHT: return GbButtonRight;
				case SDLK_LEFT: return GbButtonLeft;
				case SDLK_UP: return GbButtonUp;
				case SDLK_DOWN: return GbButtonDown;
				default: return 0;
			}
		}

		static uint8_t ControllerBit(SDL_GameControllerButton button)
		{
			switch (button)
			{
				case SDL_CONTROLLER_BUTTON_A: return GbButtonA;
				case SDL_CONTROLLER_BUTTON_B: return GbButtonB;
				case SDL_CONTROLLER_BUTTON_START: return GbButtonStart;
				case SDL_CONTROLLER_BUTTON_BACK: return GbButtonSelect;
				case SDL_CONTROLLER_BUTTON_DPAD_UP: return GbButtonUp;
				case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return GbButtonDown;
				case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return GbButtonLeft;
				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return GbButtonRight;
				default: return 0;
			}
		}
	};

	// ---------------------------------------------------------------------------------------
	// The frame loops
	// ---------------------------------------------------------------------------------------

	static const double GbaFrameMilliseconds = 1000.0 * 228.0 * 1232.0 / (double)CyclesPerSecond;
	static const double GbFrameMilliseconds = 1000.0 * 154.0 * 456.0 / 4194304.0;

	// A sound device that stops calling the callback (it was removed, its driver stalled) must not
	// stop the machine: the frame loop never waits for it, and the mixer buffer's own drop rule is
	// what keeps the delay bounded in that case.
	static const int SoundCatchUpFrames = 4;

	/// <summary>
	/// Hand everything the machine mixed for the frame it just ran to the mixer buffer: the core
	/// queues its samples (`System::ReadAudio`) and the frontend pushes them, so the device always
	/// gets a whole frame at a time and the core's queue stays empty. With `discard` the samples
	/// are dropped instead, which is what fast forward wants (no stale audio when it ends).
	/// </summary>
	template <typename System>
	static void DrainFrameAudio(System& system, Host& host, std::vector<int16_t>& scratch, bool discard)
	{
		const int Chunk = 2048;			// frames per call; one frame of the machine is ~549

		scratch.resize((size_t)Chunk * 2);

		for (;;)
		{
			int got = system.ReadAudio(scratch.data(), Chunk);

			if (got <= 0)
			{
				break;
			}

			if (!discard)
			{
				host.PushAudio(scratch.data(), got);
			}
		}
	}

	int RunSdlFrontend(const std::string& romPath, bool linkMode, const GbaSettings& settings)
	{
		SetLogSink(FrontendLog, nullptr);

		GbaSystem system;
		system.ApplySettings(settings);

		std::string error;

		if (!romPath.empty())
		{
			if (!system.LoadRomFile(romPath, error))
			{
				printf("gba: cannot load the ROM: %s\n", error.c_str());
				return 2;
			}
		}

		if (linkMode && !system.RomLoaded())
		{
			printf("gba: no cartridge, the boot ROM link driver is in charge\n");
		}

		system.Reset();

		printf("gba: %s\n", system.Describe().c_str());
		printf("gba: the boot ROM animation runs for %i frames\n", BootRom::GbaAnimationFrames());

		Host host;

		if (!host.Open("pureikyubu GBA", ScreenWidth, ScreenHeight, settings))
		{
			return 3;
		}

		system.SetSampleRate(host.sampleRate);

		// The debug interface of the portable machine and the new debugger. The node is
		// registered here (and not for the whole program), because only this path runs a machine
		// the portable commands can answer for. The debugger window itself is opened only when
		// the `emulation.debugger` setting asks for it; F2 opens and closes it either way.
		SetDebugMachine(&system);
		DebugStart(settings.debugger);

		GbaInput input(settings);

		std::vector<int16_t> samples;
		Hotkeys keys;
		keys.fullscreen = settings.fullscreen;
		int slot = 0;					// the save state slot F3/F4 use
		bool running = true;
		uint32_t paceStart = SDL_GetTicks();
		uint64_t paceFrames = 0;
		uint64_t frames = 0;

		while (running)
		{
			// The frontend's own keys. The two toggles keep the state they were in; every other
			// request is for this iteration only, so the struct starts as a copy of the toggles
			// and the poll fills in what the user asked for now.
			Hotkeys pressed;
			pressed.fastForward = keys.fastForward;
			pressed.fullscreen = keys.fullscreen;

			// The debugger window has its own events; the ones that are not its own are put back
			// into the queue and end up in the poll loop below.
			DebugPumpEvents();

			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				host.HandleHotkey(event, pressed);

				if (!input.Handle(event))
				{
					running = false;
				}
			}

			keys = pressed;

			if (keys.debugger)
			{
				if (DebugActive())
					DebugStop();
				else
					DebugStart();
			}

			if (!running)
			{
				break;
			}

			host.ApplyFullscreen(keys.fullscreen);

			if (keys.saveBattery)
			{
				std::string saveError;
				system.SaveBattery(&saveError);
				printf("gba: battery saved%s%s\n", saveError.empty() ? "" : " - ", saveError.c_str());
			}

			// The quick save and the quick load (F3/F4, and the slot they use is picked with
			// Shift+F3/Shift+F4). Both go through the state file of the slot, so what a frontend
			// key writes is exactly what the `savestate` JDI command writes.
			if (keys.slotStep != 0)
			{
				slot = (slot + keys.slotStep + (MaxStateSlot + 1)) % (MaxStateSlot + 1);
				printf("gba: save state slot %i\n", slot);
			}

			if (keys.saveState || keys.loadState)
			{
				std::string path = system.StateFilePath(slot);
				std::string stateError;
				bool ok = keys.saveState ? system.SaveStateFile(path, &stateError)
					: system.LoadStateFile(path, &stateError);

				printf("gba: %s slot %i (%s)%s%s\n",
					keys.saveState ? "state saved to" : "state loaded from", slot, path.c_str(),
					ok ? "" : " - ", ok ? "" : stateError.c_str());
			}

			// The machine runs one frame per iteration, so its speed is the frame loop's pace -
			// `PaceFrame`, which holds the loop to the machine's own 59.7275 Hz frame period
			// whether vsync is on or off - and not the sound device's clock, which would make the
			// emulation follow whatever the device's crystal happens to do. The difference between
			// the two clocks is absorbed by the mixer instead, which plays the buffer at a slightly
			// different rate (Host::UpdateAudioClock); a buffer that ran dry is refilled with extra
			// frames here, but only while this frame still has time left, so a machine that cannot
			// keep up does not lose the display to the catch-up as well.
			if (keys.fastForward)
			{
				// Fast forward runs the machine as fast as the host allows; the sound of the
				// frames is thrown away rather than pushed, so leaving fast forward does not start
				// with seconds of stale audio and the pitch does not turn into noise.
				for (int i = 0; i < 4; i++)
				{
					system.SetPressedKeys(input.Pressed());
					system.RunFrame();
					frames++;
				}

				DrainFrameAudio(system, host, samples, true);
			}
			else
			{
				uint32_t started = SDL_GetTicks();

				system.SetPressedKeys(input.Pressed());
				system.RunFrame();
				frames++;
				DrainFrameAudio(system, host, samples, false);

				for (int extra = 1; extra < SoundCatchUpFrames && host.SoundStarving(); extra++)
				{
					if (SDL_GetTicks() - started >= (uint32_t)GbaFrameMilliseconds)
					{
						break;
					}

					system.RunFrame();
					frames++;
					DrainFrameAudio(system, host, samples, false);
				}

				host.UpdateAudioClock();
			}

			host.Present(system.FrameBuffer());

			if (keys.screenshot)
			{
				host.SaveScreenshot(system.FrameBuffer());
			}

			host.UpdateFps();

			if (settings.showFps)
			{
				char title[256];
				snprintf(title, sizeof(title), "pureikyubu GBA - %s - state %i - %.1f fps%s%s%s",
					system.RomTitle().empty()
						? (system.LinkMode() ? "link mode" : "no cartridge")
						: system.RomTitle().c_str(),
					slot,
					host.fps,
					host.AudioText().c_str(),
					keys.fastForward ? " [fast forward]" : "",
					system.Link().Peer() != nullptr ? " [linked]" : "");
				SDL_SetWindowTitle(host.window, title);
			}

			DebugFrame();
			host.PaceFrame(frames, paceStart, GbaFrameMilliseconds, paceStart, paceFrames);
		}

		// The debugger is torn down from here, not from its own window callback (see debugui2.h).
		DebugStop();

		std::string saveError;
		system.SaveBattery(&saveError);

		return 0;
	}

	int RunSdlFrontendGb(const std::string& romPath, const GbaSettings& settings, bool forceDmg)
	{
		SetLogSink(FrontendLog, nullptr);

		GbSystem system;

		GbSettings gbSettings = GbSettings::Defaults();
		gbSettings.sampleRate = settings.sampleRate;
		gbSettings.highPassFilter = settings.highPassFilter;
		gbSettings.useBootRom = settings.useCustomBootRom;
		gbSettings.logLevel = settings.logLevel;

		// The console kind: a CGB by default, with a DMG-only cartridge running on it in
		// compatibility mode (Pan Docs KEY0; the machine greys the picture because the emulator
		// does not implement the CGB's compatibility palette tables). `--gb-dmg` asks for the
		// monochrome console instead.
		if (!forceDmg)
		{
			gbSettings.cgb = true;
		}

		system.ApplySettings(gbSettings);

		std::string error;

		if (!romPath.empty())
		{
			if (!system.LoadRomFile(romPath, error))
			{
				printf("gb: cannot load the ROM: %s\n", error.c_str());
				return 2;
			}

			if (!forceDmg && !system.Cartridge().CgbCompatible())
			{
				// A DMG cartridge on a CGB: the console is a CGB, the cartridge runs in
				// compatibility mode (that is what the CGB's KEY0/CPU mode is for).
				gbSettings.cgb = true;
				system.ApplySettings(gbSettings);
			}
		}

		system.Reset();

		printf("gb: %s\n", system.Describe().c_str());

		Host host;

		if (!host.Open("pureikyubu Game Boy", GbScreenWidth, GbScreenHeight, settings))
		{
			return 3;
		}

		system.SetSampleRate(host.sampleRate);

		// The debug interface of the Game Boy and the new debugger (the same arrangement as the
		// GBA frontend above: the window opens only when `emulation.debugger` asks for it).
		SetDebugMachine(&system);
		DebugStart(settings.debugger);

		GbInput input;

		std::vector<int16_t> samples;
		Hotkeys keys;
		keys.fullscreen = settings.fullscreen;
		int slot = 0;					// the save state slot F3/F4 use
		bool running = true;
		uint32_t paceStart = SDL_GetTicks();
		uint64_t paceFrames = 0;
		uint64_t frames = 0;

		while (running)
		{
			Hotkeys pressed;
			pressed.fastForward = keys.fastForward;
			pressed.fullscreen = keys.fullscreen;

			DebugPumpEvents();

			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				host.HandleHotkey(event, pressed);

				if (!input.Handle(event))
				{
					running = false;
				}
			}

			keys = pressed;

			if (keys.debugger)
			{
				if (DebugActive())
					DebugStop();
				else
					DebugStart();
			}

			if (!running)
			{
				break;
			}

			host.ApplyFullscreen(keys.fullscreen);

			if (keys.saveBattery)
			{
				std::string saveError;
				system.SaveBattery(&saveError);
				printf("gb: battery saved%s%s\n", saveError.empty() ? "" : " - ", saveError.c_str());
			}

			// The Game Boy's quick save and quick load, exactly as the GBA's above: F3 writes the
			// current slot, F4 reads it back and Shift+F3/Shift+F4 pick another one. The state
			// carries the console kind, so a colour state is never loaded into a monochrome
			// machine (the machine says so instead).
			if (keys.slotStep != 0)
			{
				slot = (slot + keys.slotStep + (MaxStateSlot + 1)) % (MaxStateSlot + 1);
				printf("gb: save state slot %i\n", slot);
			}

			if (keys.saveState || keys.loadState)
			{
				std::string path = system.StateFilePath(slot);
				std::string stateError;
				bool ok = keys.saveState ? system.SaveStateFile(path, &stateError)
					: system.LoadStateFile(path, &stateError);

				printf("gb: %s slot %i (%s)%s%s\n",
					keys.saveState ? "state saved to" : "state loaded from", slot, path.c_str(),
					ok ? "" : " - ", ok ? "" : stateError.c_str());
			}

			// The machine runs one frame per iteration and the mixer absorbs the difference
			// between the frame loop's clock and the sound device's, exactly as in the GBA loop
			// above (the Game Boy's frame is 59.7275 Hz too).
			if (keys.fastForward)
			{
				for (int i = 0; i < 4; i++)
				{
					system.SetPressedKeys(input.Pressed());
					system.RunFrame();
					frames++;
				}

				DrainFrameAudio(system, host, samples, true);
			}
			else
			{
				uint32_t started = SDL_GetTicks();

				system.SetPressedKeys(input.Pressed());
				system.RunFrame();
				frames++;
				DrainFrameAudio(system, host, samples, false);

				for (int extra = 1; extra < SoundCatchUpFrames && host.SoundStarving(); extra++)
				{
					if (SDL_GetTicks() - started >= (uint32_t)GbFrameMilliseconds)
					{
						break;
					}

					system.RunFrame();
					frames++;
					DrainFrameAudio(system, host, samples, false);
				}

				host.UpdateAudioClock();
			}

			host.Present(system.FrameBuffer());

			if (keys.screenshot)
			{
				host.SaveScreenshot(system.FrameBuffer());
			}

			host.UpdateFps();

			if (settings.showFps)
			{
				char title[256];
				snprintf(title, sizeof(title), "pureikyubu Game Boy - %s - %s - state %i - %.1f fps%s%s",
					system.Cgb() ? "CGB" : "DMG",
					system.RomTitle().empty() ? "no cartridge" : system.RomTitle().c_str(),
					slot,
					host.fps,
					host.AudioText().c_str(),
					keys.fastForward ? " [fast forward]" : "");
				SDL_SetWindowTitle(host.window, title);
			}

			DebugFrame();
			host.PaceFrame(frames, paceStart, GbFrameMilliseconds, paceStart, paceFrames);
		}

		DebugStop();

		std::string saveError;
		system.SaveBattery(&saveError);

		return 0;
	}
}
