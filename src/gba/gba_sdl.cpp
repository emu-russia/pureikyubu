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

		void Present(const uint32_t* pixels)
		{
			SDL_UpdateTexture(texture, nullptr, pixels, width * 4);
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

		/// <summary>
		/// True while the sound device wants another frame of the machine: the buffer is at or
		/// past its high water mark when the machine has produced more than the device has
		/// played. Without a device the machine is never held back.
		/// </summary>
		bool WantsSoundFrame() const
		{
			if (audio == 0 || !audioEnabled)
			{
				return true;
			}

			return sound.WantsFrame();
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
		/// The sound status for the window title: the delay the buffer adds, and the counters that
		/// say the sound is not keeping up (a period the buffer could not fill, audio that had to
		/// be thrown away to keep the delay bounded).
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

			if (sound.Underruns() > 0 || sound.Drops() > 0)
			{
				snprintf(text, sizeof(text), " [%u gaps, %u drops]",
					sound.Underruns(), sound.Drops());
				status += text;
			}

			return status;
		}

		void HandleHotkey(SDL_Keycode key, bool down, bool& fastForward, bool& fullscreen, bool& screenshot, bool& saveNow, bool& debugger)
		{
			if (!down)
			{
				return;
			}

			switch (key)
			{
				case SDLK_F1: fastForward = !fastForward; break;
				case SDLK_F5: saveNow = true; break;
				case SDLK_F11: fullscreen = !fullscreen; break;
				case SDLK_F12: screenshot = true; break;
				case SDLK_F2: debugger = true; break;
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

		/// <summary>Wait for the next frame boundary when the renderer is not paced by vsync.</summary>
		void PaceFrame(uint64_t frameIndex, uint32_t startTicks, double frameMilliseconds, uint32_t& paceStart, uint64_t& paceFrames)
		{
			if (vsync)
			{
				return;
			}

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

		bool Handle(const SDL_Event& event, bool& fastForward, bool& fullscreen, bool& screenshot, bool& saveNow)
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

		bool Handle(const SDL_Event& event, bool& fastForward, bool& fullscreen, bool& screenshot, bool& saveNow)
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
	// stop the machine: after this long without a frame the machine runs anyway, and the mixer
	// buffer's own drop rule is what keeps the delay bounded in that case.
	static const uint32_t SoundStallMilliseconds = 250;

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
		bool fastForward = false;
		bool fullscreen = settings.fullscreen;
		bool running = true;
		uint32_t paceStart = SDL_GetTicks();
		uint32_t lastMachineFrame = SDL_GetTicks();
		uint64_t paceFrames = 0;
		uint64_t frames = 0;

		while (running)
		{
			bool screenshot = false;
			bool saveNow = false;
			bool debugger = false;

			// The debugger window has its own events; the ones that are not its own are put back
			// into the queue and end up in the poll loop below.
			DebugPumpEvents();

			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				host.HandleHotkey(event.type == SDL_KEYDOWN ? event.key.keysym.sym : SDLK_UNKNOWN,
					event.type == SDL_KEYDOWN, fastForward, fullscreen, screenshot, saveNow, debugger);

				if (!input.Handle(event, fastForward, fullscreen, screenshot, saveNow))
				{
					running = false;
				}
			}

			if (debugger)
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

			host.ApplyFullscreen(fullscreen);

			if (saveNow)
			{
				std::string saveError;
				system.SaveBattery(&saveError);
				printf("gba: battery saved%s%s\n", saveError.empty() ? "" : " - ", saveError.c_str());
			}

			// The frame runs while the mixer buffer has room for it (see Host::WantsSoundFrame).
			// With vsync on, the display's refresh rate (usually 60.00 Hz) is not the GBA's
			// 59.7275 Hz, so the machine mixes 0.46 % more sound than the device plays; the buffer
			// absorbs that by throwing away the few samples per frame that do not fit (see
			// gba_audio.h), which keeps the delay at the cushion and leaves the picture alone. The
			// gate is the backstop for a machine that is genuinely ahead - a burst pushed after the
			// frontend was stalled, a device that stopped calling the callback - so that the delay
			// can never grow past the buffer. `SoundStallMilliseconds` is the way out when the
			// device itself has stopped.
			uint32_t now = SDL_GetTicks();
			bool stalled = (now - lastMachineFrame) > SoundStallMilliseconds;

			if (fastForward || stalled || host.WantsSoundFrame())
			{
				lastMachineFrame = now;

				system.SetPressedKeys(input.Pressed());
				system.RunFrame();
				frames++;

				if (fastForward)
				{
					// Fast forward runs the machine as fast as the host allows; the sound of the
					// extra frames is thrown away rather than pushed, so leaving fast forward does
					// not start with seconds of stale audio and the pitch does not turn into noise.
					for (int i = 0; i < 3; i++)
					{
						system.RunFrame();
						frames++;
					}

					DrainFrameAudio(system, host, samples, true);
				}
				else
				{
					DrainFrameAudio(system, host, samples, false);
				}
			}

			host.Present(system.FrameBuffer());

			if (screenshot)
			{
				host.SaveScreenshot(system.FrameBuffer());
			}

			host.UpdateFps();

			if (settings.showFps)
			{
				char title[256];
				snprintf(title, sizeof(title), "pureikyubu GBA - %s - %.1f fps%s%s%s",
					system.RomTitle().empty()
						? (system.LinkMode() ? "link mode" : "no cartridge")
						: system.RomTitle().c_str(),
					host.fps,
					host.AudioText().c_str(),
					fastForward ? " [fast forward]" : "",
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
		bool fastForward = false;
		bool fullscreen = settings.fullscreen;
		bool running = true;
		uint32_t paceStart = SDL_GetTicks();
		uint32_t lastMachineFrame = SDL_GetTicks();
		uint64_t paceFrames = 0;
		uint64_t frames = 0;

		while (running)
		{
			bool screenshot = false;
			bool saveNow = false;
			bool debugger = false;

			DebugPumpEvents();

			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				host.HandleHotkey(event.type == SDL_KEYDOWN ? event.key.keysym.sym : SDLK_UNKNOWN,
					event.type == SDL_KEYDOWN, fastForward, fullscreen, screenshot, saveNow, debugger);

				if (!input.Handle(event, fastForward, fullscreen, screenshot, saveNow))
				{
					running = false;
				}
			}

			if (debugger)
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

			host.ApplyFullscreen(fullscreen);

			if (saveNow)
			{
				std::string saveError;
				system.SaveBattery(&saveError);
				printf("gb: battery saved%s%s\n", saveError.empty() ? "" : " - ", saveError.c_str());
			}

			// The mixer buffer and the sound device, exactly as in the GBA loop above (the Game Boy
			// frame is 59.7275 Hz too).
			uint32_t now = SDL_GetTicks();
			bool stalled = (now - lastMachineFrame) > SoundStallMilliseconds;

			if (fastForward || stalled || host.WantsSoundFrame())
			{
				lastMachineFrame = now;

				system.SetPressedKeys(input.Pressed());
				system.RunFrame();
				frames++;

				if (fastForward)
				{
					for (int i = 0; i < 3; i++)
					{
						system.RunFrame();
						frames++;
					}

					DrainFrameAudio(system, host, samples, true);
				}
				else
				{
					DrainFrameAudio(system, host, samples, false);
				}
			}

			host.Present(system.FrameBuffer());

			if (screenshot)
			{
				host.SaveScreenshot(system.FrameBuffer());
			}

			host.UpdateFps();

			if (settings.showFps)
			{
				char title[256];
				snprintf(title, sizeof(title), "pureikyubu Game Boy - %s - %s - %.1f fps%s%s",
					system.Cgb() ? "CGB" : "DMG",
					system.RomTitle().empty() ? "no cartridge" : system.RomTitle().c_str(),
					host.fps,
					host.AudioText().c_str(),
					fastForward ? " [fast forward]" : "");
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
