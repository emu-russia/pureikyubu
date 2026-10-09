/*

# The stand-alone GBA settings

The emulator can run as a Game Boy Advance instead of a GameCube (`--gba`, or a file with a Game Boy
extension on the command line). That machine is a separate one inside the same executable, and it
keeps its settings in the pair of documents the rest of the emulator keeps its own in:
`Data/DefaultGBASettings.json` is the shipped one and `Data/GBASettings.json` is the user's, merged
over the defaults member by member. This window is where those settings are edited from the
console's own user interface, so neither file has to be hand written:

  * the members are the sections of `GBA::GbaSettings` (`src/gba/gba_settings.h`): boot, video,
    audio, input, link and emulation, one collapsing section per heading;
  * the pair is read when the window is opened for the first time - through the GBA module's own
    reader (`GBA::LoadSettings`), the same call the front end makes - and the user's file is
    written back when "Save" is asked for (`GbaSettings::Save`), so what it writes is byte for byte
    the document that front end merges over the defaults;
  * the key bindings are edited here too, and "Default keys" is the module's own default layout
    (`GbaSettings::Defaults`), which is the one the shipped defaults carry: the **A** of a Game Boy
    Advance is the *right-hand* button of the two, so its key (`X`) sits to the right of B's (`Z`).
    The window says so, because a layout that has them the other way round is the one mistake a
    per-key editor invites.

The window is a module of its own for the same reason the console's settings window is: the front
end (`uisdl.cpp`) draws it, opens it from its menu and hands it the SDL events of a capture, and
knows nothing about what is inside it.

*/

#include "pch.h"
#include "uisettingsgba.h"
#include "gba/gba_settings.h"
#include "gba/gba_sdl.h"
#include "../thirdparty/imgui-filebrowser/imfilebrowser.h"

// ---------------------------------------------------------------------------
// State

static bool                 gba_open = false;
static bool                 gba_loaded = false;
static GBA::GbaSettings       gba_settings;      // the working copy the window edits
static GBA::GbaSettingsFiles  gba_files;         // the shipped defaults it reads and the user's file it writes
static std::string            gba_status;        // the last load or save result, shown in the window

//! The binding a capture was armed for (an index into `gba_settings.keys`), or -1.
static int                  gba_capture_binding = -1;
static bool                 gba_capture_active = false;

static ImGui::FileBrowser   gba_bios_dialog(ImGuiFileBrowserFlags_CloseOnEsc);
static bool                 gba_dialogs_ready = false;

//! The machine the BIOS browser that is open (or about to be opened) is picking an image for. The
//! three portable machines have a boot image each, so the row that opened the browser says which.
enum class GbaBiosTarget
{
	Gba = 0,
	Dmg,
	Cgb,
};

static GbaBiosTarget        gba_bios_target = GbaBiosTarget::Gba;

// ---------------------------------------------------------------------------
// The property grid (the same shape the console's settings window uses)

#define GBA_PROP_NAME_WIDTH     220.0f

static bool GbaPropertyGrid(const char* id)
{
	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersInnerV))
	{
		return false;
	}

	ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthFixed, GBA_PROP_NAME_WIDTH);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

	return true;
}

static void GbaPropertyRow(const char* name)
{
	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(name);

	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::PushID(name);
}

static void GbaPropertyRowEnd()
{
	ImGui::PopID();
}

static void GbaPropertyGridEnd()
{
	ImGui::EndTable();
}

// ---------------------------------------------------------------------------
// The file

static void GbaReload()
{
	std::string error;

	gba_settings = GBA::GbaSettings::Defaults();
	gba_files = GBA::FindSettingsFiles();

	if (GBA::LoadSettings(gba_files, gba_settings, &error))
	{
		// The pair is what was read: the user's file is merged over the defaults, and a file that
		// is not there yet is not an error (the defaults of the shipped one are in use).
		gba_status = "loaded " + gba_files.defaults + ", overridden by " + gba_files.user;
	}
	else
	{
		gba_status = error;
	}

	gba_loaded = true;
}

static void GbaSave()
{
	if (gba_files.user.empty())
	{
		gba_files = GBA::FindSettingsFiles();
	}

	std::string error;

	if (gba_settings.Save(gba_files.user, &error))
	{
		gba_status = "saved " + gba_files.user;
	}
	else
	{
		gba_status = "cannot write " + gba_files.user + ": " + error;
	}
}

// ---------------------------------------------------------------------------
// The pages

/* The boot image of one of the three portable machines, as the working copy holds it (issue #468:
   each of them has one of its own, and the one BIOS field the window used to have did not say
   which machine it belonged to). */
static std::string& GbaBiosPath(GbaBiosTarget target)
{
	switch (target)
	{
		case GbaBiosTarget::Dmg: return gba_settings.dmgBiosPath;
		case GbaBiosTarget::Cgb: return gba_settings.cgbBiosPath;
		default: return gba_settings.biosPath;
	}
}

/* What a machine's boot image is called, for the title of the browser and the status line. */
static const char* GbaBiosName(GbaBiosTarget target)
{
	switch (target)
	{
		case GbaBiosTarget::Dmg: return "Game Boy (DMG) boot ROM";
		case GbaBiosTarget::Cgb: return "Game Boy Color (CGB) boot ROM";
		default: return "Game Boy Advance BIOS";
	}
}

/* One boot image row: the path, the button that opens the browser and the button that puts the
   built-in image back. */
static void GbaBiosRow(const char* label, GbaBiosTarget target)
{
	GbaPropertyRow(label);
	{
		std::string& path = GbaBiosPath(target);

		char buf[0x400];
		snprintf(buf, sizeof(buf), "%s", path.c_str());

		// The field is typed into *or* filled by the browser, and the two buttons of the row are given
		// the width that is left of it: the editor of a property row takes the whole column by
		// default (see GbaPropertyRow), which would push them past the edge and clip them away - the
		// row then looks like a field with no way to pick a file at all. The width is measured rather
		// than guessed, so a resized window keeps both buttons inside the row.
		const float buttonWidth = 100.0f;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		float fieldWidth = ImGui::GetContentRegionAvail().x - (buttonWidth * 2.0f + spacing * 2.0f);

		if (fieldWidth < 60.0f)
		{
			fieldWidth = 60.0f;
		}

		ImGui::SetNextItemWidth(fieldWidth);
		ImGui::InputText("##v", buf, sizeof(buf));
		path = buf;

		ImGui::SameLine();

		if (ImGui::Button("Choose...", ImVec2(buttonWidth, 0)))
		{
			gba_bios_target = target;
			gba_bios_dialog.SetTitle(std::string("Select the ") + GbaBiosName(target));
			gba_bios_dialog.Open();
		}

		ImGui::SameLine();

		if (ImGui::Button("Built-in", ImVec2(buttonWidth, 0)))
		{
			path.clear();
		}
	}
	GbaPropertyRowEnd();
}

static void GbaPageBoot()
{
	if (!GbaPropertyGrid("gba_boot"))
	{
		return;
	}

	GbaBiosRow("GBA BIOS", GbaBiosTarget::Gba);
	GbaBiosRow("DMG boot ROM", GbaBiosTarget::Dmg);
	GbaBiosRow("CGB boot ROM", GbaBiosTarget::Cgb);

	GbaPropertyRow("Built-in boot animation");
	ImGui::Checkbox("##v", &gba_settings.useCustomBootRom);
	GbaPropertyRowEnd();

	GbaPropertyRow("Skip the boot animation");
	ImGui::Checkbox("##v", &gba_settings.skipBootAnimation);
	GbaPropertyRowEnd();

	GbaPropertyRow("BIOS calls in the host");
	ImGui::Checkbox("##v", &gba_settings.hleBios);
	GbaPropertyRowEnd();

	GbaPropertyGridEnd();
	ImGui::TextWrapped("An empty field is the built-in boot ROM. The Game Boy Advance BIOS is a "
		"16 KByte image and the machine runs it from address 0 instead, which is what a Game Pak "
		"that expects the BIOS at the original vectors needs. The Game Boy's boot ROM is its BIOS: "
		"256 bytes on the DMG and 2304 on the CGB, and the machine the cartridge is about to run on "
		"picks its own image.");
}

static void GbaPageVideo()
{
	if (!GbaPropertyGrid("gba_video"))
	{
		return;
	}

	GbaPropertyRow("Scale");
	ImGui::SliderInt("##v", &gba_settings.videoScale, 1, 6);
	GbaPropertyRowEnd();

	GbaPropertyRow("Fullscreen");
	ImGui::Checkbox("##v", &gba_settings.fullscreen);
	GbaPropertyRowEnd();

	GbaPropertyRow("Vertical sync");
	ImGui::Checkbox("##v", &gba_settings.vsync);
	GbaPropertyRowEnd();

	GbaPropertyRow("Integer scale");
	ImGui::Checkbox("##v", &gba_settings.integerScale);
	GbaPropertyRowEnd();

	GbaPropertyRow("Show the frame rate");
	ImGui::Checkbox("##v", &gba_settings.showFps);
	GbaPropertyRowEnd();

	GbaPropertyRow("LCD ghosting");
	ImGui::Checkbox("##v", &gba_settings.lcdEffect);
	GbaPropertyRowEnd();

	GbaPropertyRow("Frame skip");
	ImGui::Checkbox("##v", &gba_settings.frameSkip);
	GbaPropertyRowEnd();

	GbaPropertyGridEnd();
}

static void GbaPageAudio()
{
	if (!GbaPropertyGrid("gba_audio"))
	{
		return;
	}

	GbaPropertyRow("Sound");
	ImGui::Checkbox("##v", &gba_settings.audioEnabled);
	GbaPropertyRowEnd();

	GbaPropertyRow("Sample rate");
	{
		static const int rates[] = { 32768, 44100, 48000 };
		static const char* names[] = { "32768 Hz", "44100 Hz", "48000 Hz" };

		int current = 0;

		for (int i = 0; i < 3; i++)
		{
			if (rates[i] == gba_settings.sampleRate)
			{
				current = i;
			}
		}

		if (ImGui::BeginCombo("##v", names[current]))
		{
			for (int i = 0; i < 3; i++)
			{
				if (ImGui::Selectable(names[i], current == i))
				{
					gba_settings.sampleRate = rates[i];
				}
			}

			ImGui::EndCombo();
		}
	}
	GbaPropertyRowEnd();

	GbaPropertyRow("Volume");
	ImGui::SliderInt("##v", &gba_settings.volume, 0, 100);
	GbaPropertyRowEnd();

	GbaPropertyRow("High pass filter");
	ImGui::Checkbox("##v", &gba_settings.highPassFilter);
	GbaPropertyRowEnd();

	GbaPropertyGridEnd();
	ImGui::TextWrapped("The high pass filter is the one a Game Boy's own amplifier has: off, the "
		"sound is the two DACs' raw sum.");
}

/* The keyboard layout: one row per action, with the key it is bound to and a capture button. */
static void GbaPageInput()
{
	ImGui::TextWrapped("A Game Boy Advance has the A button to the right of B: the default layout "
		"keeps that on the keyboard too (A is X, B is Z), and so does any layout built here. The "
		"Game Boy (DMG/CGB) takes the eight bindings it shares with the Advance from this same list "
		"- A, B, SELECT, START, RIGHT, LEFT, UP and DOWN - so the two machines answer the same key "
		"with the same button; a key bound to the Advance's R, L or SPEED does nothing on it.");

	if (ImGui::Button("Default keys"))
	{
		GBA::GbaSettings defaults = GBA::GbaSettings::Defaults();
		gba_settings.keys = defaults.keys;
	}

	ImGui::SameLine();

	if (ImGui::Button("Clear every binding"))
	{
		for (auto& binding : gba_settings.keys)
		{
			binding.key.clear();
		}
	}

	if (!GbaPropertyGrid("gba_input"))
	{
		return;
	}

	for (size_t i = 0; i < gba_settings.keys.size(); i++)
	{
		const std::string& action = gba_settings.keys[i].action;

		GbaPropertyRow(action.c_str());
		{
			bool capturing = gba_capture_active && gba_capture_binding == (int)i;

			// The two buttons are pinned to the right end of the row, so a long key name ("Left
			// Shift", "Keypad 5") cannot push them out of it - the same reason the BIOS row measures
			// its field.
			const float bindWidth = 90.0f;
			const float clearWidth = 80.0f;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			float buttons = ImGui::GetContentRegionAvail().x -
				(bindWidth + clearWidth + spacing);

			if (buttons < 80.0f)
			{
				buttons = 80.0f;
			}

			ImGui::TextUnformatted(capturing ? "(press a key)"
				: (gba_settings.keys[i].key.empty() ? "unbound" : gba_settings.keys[i].key.c_str()));

			ImGui::SameLine(buttons);

			char label[0x40];
			snprintf(label, sizeof(label), capturing ? "Cancel##bind%u" : "Bind...##bind%u",
				(unsigned)i);

			if (ImGui::Button(label, ImVec2(bindWidth, 0)))
			{
				if (capturing)
				{
					gba_capture_binding = -1;
					gba_capture_active = false;
				}
				else
				{
					gba_capture_binding = (int)i;
					gba_capture_active = true;
				}
			}

			ImGui::SameLine();

			snprintf(label, sizeof(label), "Clear##clear%u", (unsigned)i);

			if (ImGui::Button(label, ImVec2(clearWidth, 0)))
			{
				gba_settings.keys[i].key.clear();
			}
		}
		GbaPropertyRowEnd();
	}

	GbaPropertyGridEnd();
}

static void GbaPageLink()
{
	if (!GbaPropertyGrid("gba_link"))
	{
		return;
	}

	GbaPropertyRow("Link port");
	ImGui::Checkbox("##v", &gba_settings.linkEnabled);
	GbaPropertyRowEnd();

	GbaPropertyRow("Wait for another instance");
	ImGui::Checkbox("##v", &gba_settings.linkServer);
	GbaPropertyRowEnd();

	GbaPropertyRow("Address");
	{
		char buf[0x100];
		snprintf(buf, sizeof(buf), "%s", gba_settings.linkAddress.c_str());

		ImGui::InputText("##v", buf, sizeof(buf));
		gba_settings.linkAddress = buf;
	}
	GbaPropertyRowEnd();

	GbaPropertyRow("Players");
	ImGui::SliderInt("##v", &gba_settings.linkPlayers, 2, 4);
	GbaPropertyRowEnd();

	GbaPropertyGridEnd();
}

static void GbaPageEmulation()
{
	if (!GbaPropertyGrid("gba_emulation"))
	{
		return;
	}

	GbaPropertyRow("Real time clock");
	ImGui::Checkbox("##v", &gba_settings.rtcEnabled);
	GbaPropertyRowEnd();

	GbaPropertyRow("Boot with no cartridge");
	ImGui::Checkbox("##v", &gba_settings.bootWithNoCartridge);
	GbaPropertyRowEnd();

	GbaPropertyRow("Open the debugger");
	ImGui::Checkbox("##v", &gba_settings.debugger);
	GbaPropertyRowEnd();

	GbaPropertyRow("Save directory");
	{
		char buf[0x400];
		snprintf(buf, sizeof(buf), "%s", gba_settings.saveDirectory.c_str());

		ImGui::InputText("##v", buf, sizeof(buf));
		gba_settings.saveDirectory = buf;
	}
	GbaPropertyRowEnd();

	GbaPropertyRow("Log level");
	{
		static const char* names[] = { "errors", "warnings", "info", "debug" };
		int level = gba_settings.logLevel;

		if (level < 0) level = 0;
		if (level > 3) level = 3;

		if (ImGui::BeginCombo("##v", names[level]))
		{
			for (int i = 0; i < 4; i++)
			{
				if (ImGui::Selectable(names[i], level == i))
				{
					gba_settings.logLevel = i;
				}
			}

			ImGui::EndCombo();
		}
	}
	GbaPropertyRowEnd();

	GbaPropertyGridEnd();
	ImGui::TextWrapped("An empty save directory is the directory of the cartridge, which is where "
		"a Game Pak's own save memory is written.");
}

// ---------------------------------------------------------------------------
// The window

void UiGbaSettingsOpen()
{
	if (!gba_loaded)
	{
		GbaReload();
	}

	gba_open = true;
}

bool UiGbaSettingsCaptureActive()
{
	return gba_capture_active;
}

bool UiGbaSettingsSdlEvent(const SDL_Event& event, uint32_t mainWindowID)
{
	if (!gba_capture_active || gba_capture_binding < 0)
	{
		return false;
	}

	if (event.type != SDL_KEYDOWN || event.key.windowID != mainWindowID)
	{
		return false;
	}

	// The capture takes the next key press as the binding of the action it was armed for, and Esc
	// drops it: the action keeps the key it had. A modifier on its own cannot be a binding, the same
	// way the console's pads refuse one.
	SDL_Scancode scancode = event.key.keysym.scancode;

	if (scancode == SDL_SCANCODE_ESCAPE)
	{
		gba_capture_binding = -1;
		gba_capture_active = false;
		return true;
	}

	switch (scancode)
	{
		case SDL_SCANCODE_LSHIFT:
		case SDL_SCANCODE_RSHIFT:
		case SDL_SCANCODE_LCTRL:
		case SDL_SCANCODE_RCTRL:
		case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT:
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI:
			return true;
		default:
			break;
	}

	if (gba_capture_binding < (int)gba_settings.keys.size())
	{
		gba_settings.keys[gba_capture_binding].key = SDL_GetKeyName(event.key.keysym.sym);
	}

	gba_capture_binding = -1;
	gba_capture_active = false;

	return true;
}

void UiGbaSettingsFrame()
{
	if (!gba_dialogs_ready)
	{
		// The title is set by the row that opens the browser: the three machines have a boot image
		// each, and the row says which one the selection is for.
		gba_bios_dialog.SetTypeFilters({ ".bin", ".rom", ".*" });
		gba_dialogs_ready = true;
	}

	if (!gba_open)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(720, 560), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Stand-alone GBA", &gba_open))
	{
		if (gba_capture_active)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
				"Waiting for a key: press the control to bind, Esc to drop it.");
		}

		ImGui::TextWrapped("The settings of the Game Boy Advance the emulator runs by itself "
			"(the --gba mode). The shipped defaults and the user's own file are the pair below, and "
			"they are the pair that front end loads - a saved change is in effect for the next such "
			"run.");

		ImGui::Text("Defaults: %s", gba_files.defaults.c_str());
		ImGui::Text("User file: %s", gba_files.user.c_str());

		if (!gba_status.empty())
		{
			ImGui::Text("(%s)", gba_status.c_str());
		}

		ImGui::Separator();

		if (ImGui::BeginChild("gba_pages"))
		{
			if (ImGui::CollapsingHeader("Boot", ImGuiTreeNodeFlags_DefaultOpen))
			{
				GbaPageBoot();
			}

			if (ImGui::CollapsingHeader("Video"))
			{
				GbaPageVideo();
			}

			if (ImGui::CollapsingHeader("Audio"))
			{
				GbaPageAudio();
			}

			if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen))
			{
				GbaPageInput();
			}

			if (ImGui::CollapsingHeader("Link"))
			{
				GbaPageLink();
			}

			if (ImGui::CollapsingHeader("Emulation"))
			{
				GbaPageEmulation();
			}
		}

		ImGui::EndChild();

		ImGui::Separator();

		if (ImGui::Button("Save", ImVec2(80, 0)))
		{
			GbaSave();
		}

		ImGui::SameLine();

		if (ImGui::Button("Reload", ImVec2(80, 0)))
		{
			GbaReload();
		}

		ImGui::SameLine();

		if (ImGui::Button("Restore the defaults", ImVec2(180, 0)))
		{
			std::string error;

			// The defaults are the shipped document, not the built-in values: the two are held
			// equal by the module's own test, and a file a distributor changed is the one to
			// restore.
			if (GBA::GbaSettings::Load(gba_files.defaults, gba_settings, &error))
			{
				gba_status = "the shipped defaults are in the window, Save writes them to " + gba_files.user;
			}
			else
			{
				gba_status = error;
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Close", ImVec2(80, 0)))
		{
			gba_open = false;
			gba_capture_binding = -1;
			gba_capture_active = false;
		}
	}

	ImGui::End();

	// The browser is drawn after the window it was opened from, and this is also where its answer is
	// taken (the same two steps the console's settings window takes in settings_poll_files).
	gba_bios_dialog.Display();

	if (gba_bios_dialog.HasSelected())
	{
		GbaBiosPath(gba_bios_target) = gba_bios_dialog.GetSelected().string();
		gba_bios_dialog.ClearSelected();
		gba_status = std::string("the ") + GbaBiosName(gba_bios_target) + " is chosen, Save writes it";
	}

	if (!gba_open)
	{
		gba_capture_binding = -1;
		gba_capture_active = false;
	}
}
