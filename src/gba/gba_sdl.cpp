// The SDL2 frontend of the GBA and Game Boy emulators (see gba_sdl.h).

#include "gba_sdl.h"
#include "gba.h"
#include "gba_bootrom.h"
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
	// The SDL2 host: the window, the streaming texture and the audio queue
	// ---------------------------------------------------------------------------------------

	class Host
	{
	public:
		SDL_Window* window = nullptr;
		SDL_Renderer* renderer = nullptr;
		SDL_Texture* texture = nullptr;
		SDL_AudioDeviceID audio = 0;

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
				SDL_AudioSpec want{}, have{};
				want.freq = sampleRate;
				want.format = AUDIO_S16SYS;
				want.channels = 2;
				want.samples = 1024;

				audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

				if (audio == 0)
				{
					printf("emu: no audio device (%s); running silently\n", SDL_GetError());
					audioEnabled = false;
				}
				else
				{
					// The core follows the device's rate if it gave us another one.
					sampleRate = have.freq;
					SDL_PauseAudioDevice(audio, 0);
				}
			}

			return true;
		}

		void Present(const u32* pixels)
		{
			SDL_UpdateTexture(texture, nullptr, pixels, width * 4);
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, nullptr, nullptr);
			SDL_RenderPresent(renderer);
		}

		void QueueAudio(const s16* samples, int frames)
		{
			if (audio != 0 && frames > 0)
			{
				SDL_QueueAudio(audio, samples, (Uint32)(frames * 2 * sizeof(s16)));
			}
		}

		/// <summary>True when the audio queue is short enough to take another frame.</summary>
		bool NeedsAudio() const
		{
			if (audio == 0)
			{
				return false;
			}

			// About two frames of audio: enough to survive a hiccup, little enough to stay in sync.
			u32 target = (u32)((u64)sampleRate * 2 * 2 / 60 * 2);
			return SDL_GetQueuedAudioSize(audio) < target;
		}

		void HandleHotkey(SDL_Keycode key, bool down, bool& fastForward, bool& fullscreen, bool& screenshot, bool& saveNow)
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
			static u32 timer = 0;
			static int frames = 0;

			u32 now = SDL_GetTicks();

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

		void SaveScreenshot(const u32* pixels)
		{
			SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
				SDL_PIXELFORMAT_ARGB8888);

			if (surface == nullptr)
			{
				return;
			}

			for (int y = 0; y < height; y++)
			{
				memcpy((u8*)surface->pixels + y * surface->pitch, pixels + (size_t)y * width,
					(size_t)width * 4);
			}

			char name[128];
			snprintf(name, sizeof(name), "emu_screenshot_%03i.bmp", screenshots++);
			SDL_SaveBMP(surface, name);
			SDL_FreeSurface(surface);

			printf("emu: screenshot -> %s\n", name);
		}

		/// <summary>Wait for the next frame boundary when the renderer is not paced by vsync.</summary>
		void PaceFrame(u64 frameIndex, u32 startTicks, double frameMilliseconds, u32& paceStart, u64& paceFrames)
		{
			if (vsync)
			{
				return;
			}

			paceFrames++;
			u32 next = paceStart + (u32)(frameMilliseconds * paceFrames);
			u32 now = SDL_GetTicks();

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
		u16 pressed = 0;
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

		u16 Pressed() const { return pressed; }

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
					u16 bit = settings.KeyBitFor(SDL_GetKeyName(event.key.keysym.sym));

					if (bit != 0)
					{
						if (down) pressed |= bit; else pressed &= (u16)~bit;
					}
					break;
				}

				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP:
				{
					bool down = (event.type == SDL_CONTROLLERBUTTONDOWN);
					u16 bit = ControllerBit((SDL_GameControllerButton)event.cbutton.button);

					if (bit != 0)
					{
						if (down) pressed |= bit; else pressed &= (u16)~bit;
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
		static u16 ControllerBit(SDL_GameControllerButton button)
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
		u8 pressed = 0;
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

		u8 Pressed() const { return pressed; }

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

					u8 bit = KeyBit(event.key.keysym.sym);

					if (bit != 0)
					{
						if (down) pressed |= bit; else pressed &= (u8)~bit;
					}
					break;
				}

				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP:
				{
					bool down = (event.type == SDL_CONTROLLERBUTTONDOWN);
					u8 bit = ControllerBit((SDL_GameControllerButton)event.cbutton.button);

					if (bit != 0)
					{
						if (down) pressed |= bit; else pressed &= (u8)~bit;
					}
					break;
				}

				default:
					break;
			}

			return true;
		}

	private:
		static u8 KeyBit(SDL_Keycode key)
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

		static u8 ControllerBit(SDL_GameControllerButton button)
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

		GbaInput input(settings);

		std::vector<s16> samples;
		bool fastForward = false;
		bool fullscreen = settings.fullscreen;
		bool running = true;
		u32 paceStart = SDL_GetTicks();
		u64 paceFrames = 0;
		u64 frames = 0;

		while (running)
		{
			bool screenshot = false;
			bool saveNow = false;

			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				host.HandleHotkey(event.type == SDL_KEYDOWN ? event.key.keysym.sym : SDLK_UNKNOWN,
					event.type == SDL_KEYDOWN, fastForward, fullscreen, screenshot, saveNow);

				if (!input.Handle(event, fastForward, fullscreen, screenshot, saveNow))
				{
					running = false;
				}
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

			system.SetPressedKeys(input.Pressed());
			system.RunFrame();
			frames++;

			if (fastForward)
			{
				// Fast forward runs the machine as fast as the host allows; the audio is left to
				// run dry so the pitch does not turn into noise.
				for (int i = 0; i < 3; i++)
				{
					system.RunFrame();
					frames++;
				}
			}
			else if (host.NeedsAudio())
			{
				samples.resize(2048 * 2);
				int got = system.ReadAudio(samples.data(), 2048);
				host.QueueAudio(samples.data(), got);
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
				snprintf(title, sizeof(title), "pureikyubu GBA - %s - %.1f fps%s%s",
					system.RomTitle().empty()
						? (system.LinkMode() ? "link mode" : "no cartridge")
						: system.RomTitle().c_str(),
					host.fps,
					fastForward ? " [fast forward]" : "",
					system.Link().Peer() != nullptr ? " [linked]" : "");
				SDL_SetWindowTitle(host.window, title);
			}

			host.PaceFrame(frames, paceStart, GbaFrameMilliseconds, paceStart, paceFrames);
		}

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

		// The console kind follows the cartridge's CGB flag: a CGB-only or CGB-compatible
		// cartridge runs on a CGB, a plain DMG cartridge on a DMG - unless the user asked for one
		// of them explicitly.
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

		GbInput input;

		std::vector<s16> samples;
		bool fastForward = false;
		bool fullscreen = settings.fullscreen;
		bool running = true;
		u32 paceStart = SDL_GetTicks();
		u64 paceFrames = 0;
		u64 frames = 0;

		while (running)
		{
			bool screenshot = false;
			bool saveNow = false;

			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				host.HandleHotkey(event.type == SDL_KEYDOWN ? event.key.keysym.sym : SDLK_UNKNOWN,
					event.type == SDL_KEYDOWN, fastForward, fullscreen, screenshot, saveNow);

				if (!input.Handle(event, fastForward, fullscreen, screenshot, saveNow))
				{
					running = false;
				}
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
			}
			else if (host.NeedsAudio())
			{
				samples.resize(2048 * 2);
				int got = system.ReadAudio(samples.data(), 2048);
				host.QueueAudio(samples.data(), got);
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
				snprintf(title, sizeof(title), "pureikyubu Game Boy - %s - %s - %.1f fps%s",
					system.Cgb() ? "CGB" : "DMG",
					system.RomTitle().empty() ? "no cartridge" : system.RomTitle().c_str(),
					host.fps,
					fastForward ? " [fast forward]" : "");
				SDL_SetWindowTitle(host.window, title);
			}

			host.PaceFrame(frames, paceStart, GbFrameMilliseconds, paceStart, paceFrames);
		}

		std::string saveError;
		system.SaveBattery(&saveError);

		return 0;
	}
}
