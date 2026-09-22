/*

# Settings

Every setting of the emulator lives in one window: a vertical strip of tabs on the left and a
property grid on the right. It used to be a dialog per setting ("Options -> Settings...",
"Options -> Controllers -> Port n", "Options -> Memcards -> Slot A"), and the menu carried the
settings that had no dialog at all (everything the selector view is made of).

The tabs are:

  * "General" - the game selector: the directories it scans, the file filter and the view;
  * "GCN Hardware" - the console version and the firmware images;
  * "Controllers" - the pads: the devices of the peripheral pool on the SI bus and their bindings;
  * "Memory Cards" - the devices of the pool on the EXI bus, which are the two card slots.

A property grid is a table of two columns: the name of a property on the left and its editor on the
right (see PropertyGrid / PropertyRow). The devices are edited through the peripheral subsystem
itself (peripherals.h): the pool owns them, a device publishes its actuators and its properties, and
this window is generic over both - it asks the device what to draw instead of knowing what a pad or
a card is made of.

A setting takes effect as it is edited: a binding is used by the next poll, a card file is opened at
once, and the window is closed with "Close". That is why there is no OK/Cancel: there is nothing to
apply and nothing to drop.

The window opens its own file browsers for the files it picks (a card image, a firmware image, a
directory the selector scans), because a browser that is mid-selection belongs to the setting it was
opened for. The front end has its own for the files the emulator loads.

*/

#include "pch.h"
#include "uisettings.h"
#include "../thirdparty/imgui-filebrowser/imfilebrowser.h"

// ---------------------------------------------------------------------------
// The property grid

/* The width of the property name column; the editor fills the rest of the row */
#define PROP_NAME_WIDTH     190.0f

/* Open a property grid. The caller draws the rows and closes the grid with PropertyGridEnd. */
static bool PropertyGrid(const char* id)
{
	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersInnerV))
	{
		return false;
	}

	ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, PROP_NAME_WIDTH);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

	return true;
}

static void PropertyGridEnd()
{
	ImGui::EndTable();
}

/* Start a row: the name of the property on the left, the editor (which is drawn next, and which has
   to be named "##v" to stay label-less) on the right. Every PropertyRow is closed by PropertyRowEnd. */
static void PropertyRow(const char* name)
{
	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(name);

	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::PushID(name);
}

static void PropertyRowEnd()
{
	ImGui::PopID();
}

/* A text property: the value is written back when the edit is finished, never on a keystroke (the
   configuration is written to the disk by every setter). */
static bool PropertyText(std::string& value)
{
	char buf[0x400];
	snprintf(buf, sizeof(buf), "%s", value.c_str());

	ImGui::InputText("##v", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue);

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		value = buf;
		return true;
	}

	return false;
}

// ---------------------------------------------------------------------------
// The file browsers of the window
//
// One browser per kind of a file the window picks, so that the selection a setting started is never
// taken over by another setting.

static ImGui::FileBrowser settings_open_dialog(ImGuiFileBrowserFlags_CloseOnEsc);
static ImGui::FileBrowser settings_save_dialog(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_EnterNewFilename);
static ImGui::FileBrowser settings_dir_dialog(ImGuiFileBrowserFlags_SelectDirectory);

static const std::vector<std::string> settings_any_filter = { ".*" };
static const std::vector<std::string> settings_memcard_filter = { ".mci", ".*" };

/* What the browser that is up is picking a file for */
enum class SettingsFile
{
	None = 0,
	Bootrom,
	DspDrom,
	DspIrom,
	CardFile,       //!< the file of the card an EXI device holds
	CardNew,        //!< the file of a card that is about to be made
	SelectorPath,   //!< a directory the selector scans
};

static SettingsFile settings_file = SettingsFile::None;

/* The device and the property the file of a card belongs to (see SettingsFile::CardFile) */
static int settings_file_device = -1;
static int settings_file_prop = -1;

//! Open the browser that picks a file the window reads.
static void settings_pick_file(SettingsFile what, const char* title, const std::vector<std::string>& filters)
{
	settings_file = what;
	settings_open_dialog.SetTitle(title);
	settings_open_dialog.SetTypeFilters(filters);
	settings_open_dialog.Open();
}

//! Open the browser that picks a file the window is about to write (a new card image).
static void settings_pick_new_file(const char* title, const std::vector<std::string>& filters)
{
	settings_file = SettingsFile::CardNew;
	settings_save_dialog.SetTitle(title);
	settings_save_dialog.SetTypeFilters(filters);
	settings_save_dialog.Open();
}

// ---------------------------------------------------------------------------
// The tab

enum class SettingsTab
{
	General = 0,
	Hardware,
	Controllers,
	MemoryCards,
	Max
};

static bool         settings_open = false;
static SettingsTab  settings_tab = SettingsTab::General;

/* The directory that is selected in the list of the "General" page */
static int settings_path_selected = -1;

/* The message of the error box ("the card file cannot be made"), if there is one */
static std::string  settings_error;
static bool         settings_error_open = false;

// ---------------------------------------------------------------------------
// "General" - the game selector
//
// The selector is a view of the file system, and these are the settings of that view: what is
// scanned, what is listed and how the list is shown. All of them used to live in the
// "Options -> Selector" menu; the menu item that is left there is "File -> Refresh View".

/* One file filter checkbox. The four extensions are the four bits of the FILTER user variable. */
static void settings_filter_item(const char* label, uint32_t mask)
{
	uint32_t filter = (uint32_t)UI::Jdi->GetConfigInt(USER_FILTER, USER_UI);
	bool enabled = (filter & mask) != 0;

	if (ImGui::Checkbox(label, &enabled))
	{
		filter = enabled ? (filter | mask) : (filter & ~mask);
		UI::Jdi->SetConfigInt(USER_FILTER, (int)filter, USER_UI);
		SelectorRescan();
	}
}

static void settings_page_general()
{
	SelectorSettings selector = SelectorGetSettings();
	bool changed = false;

	if (PropertyGrid("settings_general"))
	{
		PropertyRow("Enabled");
		if (ImGui::Checkbox("##v", &selector.active))
		{
			changed = true;
		}
		PropertyRowEnd();

		PropertyRow("Small icons");
		if (ImGui::Checkbox("##v", &selector.smallIcons))
		{
			changed = true;
		}
		PropertyRowEnd();

		PropertyRow("Sort by");
		{
			static const struct { SELECTOR_SORT sort; const char* name; } sorts[] =
			{
				{ SELECTOR_SORT::Default,   "Default (by icon, then by title)" },
				{ SELECTOR_SORT::Filename,  "Filename" },
				{ SELECTOR_SORT::Title,     "Title" },
				{ SELECTOR_SORT::Size,      "Size" },
				{ SELECTOR_SORT::ID,        "Game ID" },
				{ SELECTOR_SORT::Comment,   "Comment" },
				{ SELECTOR_SORT::Unsorted,  "Unsorted" },
			};

			for (const auto& entry : sorts)
			{
				if (ImGui::RadioButton(entry.name, selector.sortBy == entry.sort))
				{
					selector.sortBy = entry.sort;
					changed = true;
				}
			}
		}
		PropertyRowEnd();

		PropertyRow("File filter");
		{
			settings_filter_item("*.dol", 0xff000000);
			ImGui::SameLine();
			settings_filter_item("*.elf", 0x00ff0000);
			ImGui::SameLine();
			settings_filter_item("*.gcm, *.rvz", 0x0000ff00);
			ImGui::SameLine();
			settings_filter_item("*.iso", 0x000000ff);
		}
		PropertyRowEnd();

		PropertyGridEnd();
	}

	if (changed)
	{
		SelectorSetSettings(selector);
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Directories the selector scans:");
	ImGui::TextDisabled("A newly loaded file adds its own directory to the list");

	ImGui::BeginChild("settings_paths", ImVec2(0, 130), true);

	for (int i = 0; i < (int)selector.paths.size(); i++)
	{
		ImGui::PushID(i);

		if (ImGui::Selectable(Util::WstringToString(selector.paths[i]).c_str(), settings_path_selected == i))
		{
			settings_path_selected = i;
		}

		ImGui::PopID();
	}

	ImGui::EndChild();

	if (ImGui::Button("Add...", ImVec2(90, 0)))
	{
		settings_file = SettingsFile::SelectorPath;
		settings_dir_dialog.SetTitle("Choose Directory");
		settings_dir_dialog.Open();
	}

	ImGui::SameLine();

	if (ImGui::Button("Remove", ImVec2(90, 0)) &&
		settings_path_selected >= 0 && settings_path_selected < (int)selector.paths.size())
	{
		SelectorRemovePath(selector.paths[settings_path_selected]);
		settings_path_selected = -1;
	}

	ImGui::SameLine();

	if (ImGui::Button("Rescan", ImVec2(90, 0)))
	{
		SelectorRescan();
	}
}

// ---------------------------------------------------------------------------
// "GCN Hardware" - the console and its firmware

struct SettingsConsoleVersion
{
	uint32_t	ver;
	const char* info;
};

/* The console versions the dialog offers (see YAGCD) */
static const SettingsConsoleVersion settings_console_version[] =
{
	{ 0x00000001, "0x00000001: Retail 1" },
	{ 0x00000002, "0x00000002: HW2 production board" },
	{ 0x00000003, "0x00000003: The latest production board" },
	{ 0x10000004, "0x10000004: 1st Devkit HW" },
	{ 0x10000005, "0x10000005: 2nd Devkit HW" },
	{ 0x10000006, "0x10000006: The latest Devkit HW" },
};

static const int settings_console_known = (int)(sizeof(settings_console_version) / sizeof(settings_console_version[0]));

/* The "User defined" entry of the console version combo: a value that is not in the table (a hand
   edited configuration) is still shown, and picking that entry keeps it. */
static const char* settings_console_other_label(uint32_t version)
{
	static char label[0x40];
	sprintf(label, "0x%08X: User defined", version);
	return label;
}

/* One firmware file: a read only path and the button that picks it. The value is read from the
   configuration every frame, so the browser (which is what changes it) has nothing to apply. */
static void settings_firmware_row(const char* label, const char* var, SettingsFile what, const char* title)
{
	std::string path = UI::Jdi->GetConfigString(var, USER_HW);
	char buf[0x400];
	snprintf(buf, sizeof(buf), "%s", path.c_str());

	ImGui::PushID(var);

	ImGui::TextUnformatted(label);
	ImGui::SetNextItemWidth(-110);
	ImGui::InputText("##path", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
	ImGui::SameLine();

	if (ImGui::Button("Choose...", ImVec2(100, 0)))
	{
		settings_pick_file(what, title, settings_any_filter);
	}

	ImGui::PopID();
}

static void settings_page_hw()
{
	int consoleVer = UI::Jdi->GetConfigInt(USER_CONSOLE, USER_HW);

	int current = -1;
	for (int i = 0; i < settings_console_known; i++)
	{
		if ((int)settings_console_version[i].ver == consoleVer)
		{
			current = i;
			break;
		}
	}

	ImGui::TextUnformatted("The emulated console:");

	if (PropertyGrid("settings_hw"))
	{
		PropertyRow("Console version");
		{
			const char* preview = (current >= 0)
				? settings_console_version[current].info
				: settings_console_other_label((uint32_t)consoleVer);

			if (ImGui::BeginCombo("##v", preview))
			{
				for (int i = 0; i < settings_console_known; i++)
				{
					if (ImGui::Selectable(settings_console_version[i].info, current == i))
					{
						UI::Jdi->SetConfigInt(USER_CONSOLE, (int)settings_console_version[i].ver, USER_HW);
					}
				}

				if (current < 0)
				{
					// The entry is only shown when the configuration holds a version the table does
					// not have; picking it keeps that value.
					ImGui::Selectable(settings_console_other_label((uint32_t)consoleVer), true);
				}

				ImGui::EndCombo();
			}
		}
		PropertyRowEnd();

		PropertyGridEnd();
	}

	ImGui::TextDisabled("Use the latest production board for most cases. Use the latest Devkit HW for\n"
		"debug purposes (to see OS reports in the debugger).");

	ImGui::Separator();

	settings_firmware_row("Bootrom file:", USER_BOOTROM, SettingsFile::Bootrom, "Choose Bootrom");
	settings_firmware_row("DSP DROM file:", USER_DSP_DROM, SettingsFile::DspDrom, "Choose DSP DROM");
	settings_firmware_row("DSP IROM file:", USER_DSP_IROM, SettingsFile::DspIrom, "Choose DSP IROM");
}

// ---------------------------------------------------------------------------
// "Controllers" and "Memory Cards" - the devices of the peripheral pool
//
// The two pages are the same: the pool, filtered to the bus of the devices a page is about. A
// device is edited where it is: the page shows the devices in a list, and the one that is selected
// in it is configured in the grid below. What the grid holds for a device comes from the device
// itself - its actuators (the controls the host drives, each with a keyboard and a game controller
// binding) and its properties.

/* The device a page shows the properties of. */
static int periph_selected = 0;

/* The binding capture: armed by a binding button, and fed by the SDL event loop (see
   UiSettingsSdlEvent). */
static int  pad_capture_device = -1;        // the pool index of the device
static int  pad_capture_actuator = -1;      // the actuator of that device
static bool pad_capture_gamepad = false;    // true: a game controller control, false: a key
static bool pad_capture_active = false;
static bool pad_capture_done = false;
static int  pad_captured_binding = 0;

/* The axis capture ignores the stick noise */
#define PAD_CAPTURE_AXIS_THRESHOLD  16384

/* The keys the capture skips, because they cannot be bound */
static bool pad_capture_ignored(SDL_Scancode scancode)
{
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
		case SDL_SCANCODE_MODE:
			return true;
		default:
			return scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12;
	}
}

static void pad_capture_abort()
{
	pad_capture_device = -1;
	pad_capture_actuator = -1;
	pad_capture_gamepad = false;
	pad_capture_active = false;
	pad_capture_done = false;
}

/* The id of the property that holds the file of a device (a memory card is an image on the host) */
static int DeviceFileProperty(PeripheralDevice* device)
{
	if (device == nullptr)
	{
		return -1;
	}

	for (int i = 0; i < device->PropertyCount(); i++)
	{
		const PeriphProperty* prop = device->Property(i);

		if (prop != nullptr && prop->kind == PERIPH_PROP_FILE)
		{
			return prop->id;
		}
	}

	return -1;
}

/* The path of a file property: it is shown, never typed, and picked with the file browser. */
static void PropertyFileRow(const char* path, int index, int prop)
{
	char buf[0x400];
	snprintf(buf, sizeof(buf), "%s", path);

	ImGui::SetNextItemWidth(-110);
	ImGui::InputText("##v", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);

	ImGui::SameLine();

	if (ImGui::Button("Choose...", ImVec2(100, 0)))
	{
		settings_file_device = index;
		settings_file_prop = prop;
		settings_pick_file(SettingsFile::CardFile, "Choose Memcard File", settings_memcard_filter);
	}
}

/* A binding button: it shows what currently drives the control, and arming the capture with it makes
   the next key press or game controller event the new binding. */
static void settings_binding_button(int index, int actuator, bool gamepad)
{
	PeripheralDevice* device = Peripherals::Instance().Device(index);

	if (device == nullptr)
	{
		return;
	}

	PeriphBindings* bindings = device->ActuatorBindings(actuator);

	if (bindings == nullptr)
	{
		return;
	}

	// The label (and with it the button id) changes while the capture runs, so push a stable id.
	ImGui::PushID(actuator * 2 + (gamepad ? 1 : 0));

	std::string name = "...";

	if (pad_capture_active && pad_capture_device == index && pad_capture_actuator == actuator &&
		pad_capture_gamepad == gamepad)
	{
		name = "?";
	}
	else if (Peripherals::Instance().Host() != nullptr)
	{
		name = Peripherals::Instance().Host()->BindingName(gamepad ? bindings->gamepad : bindings->keyboard);
	}

	if (ImGui::Button(name.c_str(), ImVec2(110, 0)))
	{
		pad_capture_device = index;
		pad_capture_actuator = actuator;
		pad_capture_gamepad = gamepad;
		pad_capture_active = true;
		pad_capture_done = false;
	}

	ImGui::PopID();
}

/* The bindings of every actuator of a device, grouped the way the device groups its controls. */
static void settings_bindings(int index, PeripheralDevice* device)
{
	if (!ImGui::BeginTable("bindings", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoHostExtendX))
	{
		return;
	}

	ImGui::TableSetupColumn("Control");
	ImGui::TableSetupColumn("Keyboard");
	ImGui::TableSetupColumn("Gamepad");
	ImGui::TableHeadersRow();

	const char* group = nullptr;

	for (int i = 0; i < device->ActuatorCount(); i++)
	{
		const PeriphActuator* actuator = device->Actuator(i);

		if (actuator == nullptr)
		{
			continue;
		}

		if (group == nullptr || strcmp(group, actuator->group) != 0)
		{
			group = actuator->group;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
			ImGui::TextUnformatted(group);
		}

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(actuator->name);

		ImGui::TableSetColumnIndex(1);
		settings_binding_button(index, i, false);

		ImGui::TableSetColumnIndex(2);
		settings_binding_button(index, i, true);
	}

	ImGui::EndTable();
}

/* The page of a bus: the devices of the pool that belong to it. */
static void settings_page_devices(int bus)
{
	Peripherals& pool = Peripherals::Instance();

	// What a new device of the page is: the model its ports are meant for.
	uint32_t model = Peripherals::DefaultModelOfPort(bus == PERIPH_BUS_SI ? PERIPH_PORT_SI0 : PERIPH_PORT_SLOTA);

	// The selection has to be a device of this page (the other page selects its own).
	PeripheralDevice* selected = pool.Device(periph_selected);

	if (selected == nullptr || Peripherals::ModelBus(selected->Type()) != bus)
	{
		periph_selected = -1;

		for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
		{
			PeripheralDevice* device = pool.Device(i);

			if (device != nullptr && Peripherals::ModelBus(device->Type()) == bus)
			{
				periph_selected = i;
				selected = device;
				break;
			}
		}
	}

	ImGui::BeginChild("periph_list", ImVec2(0, 116), true);

	if (ImGui::BeginTable("periph_table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 150);
		ImGui::TableSetupColumn("Plugged into", ImGuiTableColumnFlags_WidthFixed, 150);
		ImGui::TableHeadersRow();

		for (int i = 0; i < PERIPH_MAX_DEVICES; i++)
		{
			PeripheralDevice* device = pool.Device(i);

			if (device == nullptr || Peripherals::ModelBus(device->Type()) != bus)
			{
				continue;
			}

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::PushID(i);
			if (ImGui::Selectable(pool.Name(i).c_str(), periph_selected == i, ImGuiSelectableFlags_SpanAllColumns))
			{
				periph_selected = i;
				selected = device;
			}
			ImGui::PopID();

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(Peripherals::ModelName(device->Type()));

			ImGui::TableSetColumnIndex(2);
			int port = pool.PortOf(i);
			ImGui::TextUnformatted(port < 0 ? "nothing" : pool.PortName(port));
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();

	if (ImGui::Button("Add", ImVec2(80, 0)))
	{
		int index = pool.AddDevice(model);

		if (index >= 0)
		{
			// A device that is added gets the standard bindings of its model right away; the keyboard
			// ones only when it is the first of its kind (see Peripherals::DefaultBindings).
			pool.DefaultBindings(index, pool.FirstOfModel(index));
			pool.Device(index)->SaveConfig(index);

			periph_selected = index;
			selected = pool.Device(index);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Remove", ImVec2(80, 0)) && selected != nullptr)
	{
		pool.RemoveDevice(periph_selected);
		selected = nullptr;
	}

	if (selected == nullptr)
	{
		ImGui::TextDisabled("No device of this kind is in the pool");
		return;
	}

	ImGui::Separator();

	int port = pool.PortOf(periph_selected);
	int fileProp = DeviceFileProperty(selected);

	ImGui::PushID(periph_selected);

	if (PropertyGrid("periph_props"))
	{
		PropertyRow("Model");
		ImGui::TextUnformatted(Peripherals::ModelName(selected->Type()));
		PropertyRowEnd();

		PropertyRow("Name");
		{
			std::string name = pool.Name(periph_selected);

			if (PropertyText(name))
			{
				pool.SetName(periph_selected, name);
			}
		}
		PropertyRowEnd();

		PropertyRow("Plugged into");
		{
			std::string preview = port < 0 ? "nothing" : pool.PortName(port);

			if (ImGui::BeginCombo("##v", preview.c_str()))
			{
				if (ImGui::Selectable("nothing", port < 0))
				{
					pool.Detach(periph_selected);
				}

				for (int p = 0; p < pool.PortCount(); p++)
				{
					if (pool.PortBus(p) != bus)
					{
						continue;
					}

					if (ImGui::Selectable(pool.PortName(p), port == p))
					{
						pool.Attach(periph_selected, p);
					}
				}

				ImGui::EndCombo();
			}
		}
		PropertyRowEnd();

		// What the device itself wants to show about the way it is set up.
		for (int i = 0; i < selected->PropertyCount(); i++)
		{
			const PeriphProperty* prop = selected->Property(i);

			if (prop == nullptr)
			{
				continue;
			}

			PropertyRow(prop->name);

			switch (prop->kind)
			{
				case PERIPH_PROP_BOOL:
				{
					bool value = selected->GetProperty(prop->id) != "0";

					if (ImGui::Checkbox("##v", &value))
					{
						selected->SetProperty(prop->id, value ? "1" : "0");
					}
					break;
				}

				case PERIPH_PROP_FILE:
					PropertyFileRow(selected->GetProperty(prop->id).c_str(), periph_selected, prop->id);
					break;

				case PERIPH_PROP_INFO:
					ImGui::TextUnformatted(selected->GetProperty(prop->id).c_str());
					break;
			}

			PropertyRowEnd();
		}

		PropertyGridEnd();
	}

	// A memory card is an image on the host, and one of them may have to be made first.
	if (fileProp >= 0 && ImGui::Button("Create new memory card...", ImVec2(220, 0)))
	{
		settings_file_device = periph_selected;
		settings_file_prop = fileProp;
		settings_pick_new_file("Create Memcard File", settings_memcard_filter);
	}

	if (selected->ActuatorCount() > 0)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Bindings:");

		// The host game controller of a pad is the one the host reports under the number of the
		// socket it is in (see HostPadOf in peripherals.cpp), so the names below are what tells the
		// user which controller drives which port.
		HostInput* host = pool.Host();

		if (host == nullptr || host->GamepadCount() == 0)
		{
			ImGui::TextDisabled("No host game controller is connected");
		}
		else
		{
			for (int pad = 0; pad < host->GamepadCount(); pad++)
			{
				ImGui::TextDisabled("Controller Port %i: %s", pad + 1, host->GamepadName(pad).c_str());
			}
		}

		settings_bindings(periph_selected, selected);

		if (ImGui::Button("Defaults", ImVec2(80, 0)))
		{
			pool.DefaultBindings(periph_selected, pool.FirstOfModel(periph_selected));
			selected->SaveConfig(periph_selected);
		}

		ImGui::SameLine();

		if (ImGui::Button("Clear", ImVec2(80, 0)))
		{
			pool.ClearBindings(periph_selected);
			selected->SaveConfig(periph_selected);
		}

		if (pad_capture_active)
		{
			ImGui::SameLine();
			ImGui::TextUnformatted(pad_capture_gamepad
				? "Press a game controller button or move an axis (Esc cancels)"
				: "Press a key (Esc cancels)");
		}
	}

	ImGui::PopID();
}

/* The six card sizes, in the order of Memcard_ValidSizes, so that the entry index is the index of
   the size. */
static const std::vector<std::string>& memcard_size_labels()
{
	static std::vector<std::string> labels;

	if (labels.empty())
	{
		for (int i = 0; i < Num_Memcard_ValidSizes; i++)
		{
			char buf[0x40];
			sprintf(buf, "%i blocks (%i Kb)",
				(int)(Memcard_ValidSizes[i] / Memcard_BlockSize), (int)(Memcard_ValidSizes[i] / 1024));
			labels.push_back(buf);
		}
	}

	return labels;
}

// ---------------------------------------------------------------------------
// Making a card

/* The path the browser returned and the size that is still to be asked for */
static std::wstring card_new_file;
static bool         card_size_popup = false;
static int          card_new_size = 0;

/* The size of a new card, asked after the file browser returned the path */
static void card_size_popup_window()
{
	if (card_size_popup)
	{
		ImGui::OpenPopup("Choose Memcard Size");
	}

	if (!ImGui::BeginPopupModal("Choose Memcard Size", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	std::vector<const char*> items;
	for (const auto& label : memcard_size_labels())
	{
		items.push_back(label.c_str());
	}

	ImGui::SetNextItemWidth(200);
	ImGui::Combo("##memcard_size", &card_new_size, items.data(), (int)items.size());

	if (ImGui::Button("OK", ImVec2(80, 0)))
	{
		uint32_t size = Memcard_ValidSizes[card_new_size];

		// The id of a card is its size in megabits (see MEMCARD_ID_*)
		if (!MCCreateMemcardFile(card_new_file.c_str(), (uint16_t)(size >> 17)))
		{
			settings_error = "Cannot create the memcard file:\n" + Util::WstringToString(card_new_file);
			settings_error_open = true;
		}
		else
		{
			PeripheralDevice* device = Peripherals::Instance().Device(settings_file_device);

			if (device != nullptr && settings_file_prop >= 0)
			{
				device->SetProperty(settings_file_prop, Util::WstringToString(card_new_file));
			}
		}

		card_size_popup = false;
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel", ImVec2(80, 0)))
	{
		card_size_popup = false;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

static void settings_error_box()
{
	if (settings_error_open)
	{
		ImGui::OpenPopup("Settings");
		settings_error_open = false;
	}

	if (!ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::TextUnformatted(settings_error.c_str());

	if (ImGui::Button("OK", ImVec2(80, 0)))
	{
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// What the file browsers returned

static void settings_poll_files()
{
	settings_open_dialog.Display();

	if (settings_open_dialog.HasSelected())
	{
		std::string name = settings_open_dialog.GetSelected().string();
		settings_open_dialog.ClearSelected();

		switch (settings_file)
		{
			case SettingsFile::Bootrom:
				UI::Jdi->SetConfigString(USER_BOOTROM, name, USER_HW);
				break;

			case SettingsFile::DspDrom:
				UI::Jdi->SetConfigString(USER_DSP_DROM, name, USER_HW);
				break;

			case SettingsFile::DspIrom:
				UI::Jdi->SetConfigString(USER_DSP_IROM, name, USER_HW);
				break;

			case SettingsFile::CardFile:
			{
				PeripheralDevice* device = Peripherals::Instance().Device(settings_file_device);

				if (device != nullptr && settings_file_prop >= 0)
				{
					device->SetProperty(settings_file_prop, name);
				}
				break;
			}
		}

		settings_file = SettingsFile::None;
	}

	settings_save_dialog.Display();

	if (settings_save_dialog.HasSelected())
	{
		std::string name = settings_save_dialog.GetSelected().string();
		settings_save_dialog.ClearSelected();

		if (settings_file == SettingsFile::CardNew && !name.empty())
		{
			card_new_file = Util::StringToWstring(name);
			card_new_size = 0;
			card_size_popup = true;
		}

		settings_file = SettingsFile::None;
	}

	settings_dir_dialog.Display();

	if (settings_dir_dialog.HasSelected())
	{
		std::string name = settings_dir_dialog.GetSelected().string();
		settings_dir_dialog.ClearSelected();

		if (settings_file == SettingsFile::SelectorPath && !name.empty())
		{
			SelectorAddPath(Util::StringToWstring(name));
		}

		settings_file = SettingsFile::None;
	}
}

// ---------------------------------------------------------------------------
// The window

void UiSettingsOpen()
{
	settings_open = true;
}

bool UiSettingsCaptureActive()
{
	return pad_capture_active;
}

bool UiSettingsSdlEvent(const SDL_Event& event, uint32_t mainWindowID)
{
	if (!pad_capture_active || pad_capture_done)
	{
		return false;
	}

	// The capture takes the next key press or game controller event as the binding of the control it
	// was armed with. The events must not reach ImGui, otherwise they would also move the selector
	// cursor, trigger a menu item or navigate the interface.
	if (event.type == SDL_KEYDOWN && event.key.windowID == mainWindowID)
	{
		SDL_Scancode scancode = event.key.keysym.scancode;

		if (scancode == SDL_SCANCODE_ESCAPE)
		{
			// Esc drops the capture: the control keeps the binding it had (see pad_capture_abort).
			pad_capture_abort();
		}
		else if (!pad_capture_gamepad && !pad_capture_ignored(scancode))
		{
			pad_captured_binding = (int)scancode;
			pad_capture_done = true;
			pad_capture_active = false;
		}

		return true;
	}

	if (pad_capture_gamepad && event.type == SDL_CONTROLLERBUTTONDOWN)
	{
		if (event.cbutton.button >= 0 && event.cbutton.button < SDL_CONTROLLER_BUTTON_MAX)
		{
			pad_captured_binding = PERIPH_HOST_MAKE_BUTTON(event.cbutton.button);
			pad_capture_done = true;
			pad_capture_active = false;
		}

		return true;
	}

	if (pad_capture_gamepad && event.type == SDL_CONTROLLERAXISMOTION)
	{
		if (event.caxis.axis >= 0 && event.caxis.axis < SDL_CONTROLLER_AXIS_MAX &&
			(event.caxis.value >= PAD_CAPTURE_AXIS_THRESHOLD || event.caxis.value <= -PAD_CAPTURE_AXIS_THRESHOLD))
		{
			pad_captured_binding = PERIPH_HOST_MAKE_AXIS(event.caxis.axis, event.caxis.value > 0);
			pad_capture_done = true;
			pad_capture_active = false;
		}

		return true;
	}

	if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERAXISMOTION)
	{
		// No game controller event may navigate the user interface while a binding waits for a key
		return true;
	}

	return false;
}

void UiSettingsFrame()
{
	if (!settings_open)
	{
		return;
	}

	// Apply the binding the event loop captured
	if (pad_capture_done)
	{
		PeripheralDevice* device = Peripherals::Instance().Device(pad_capture_device);

		if (device != nullptr && pad_capture_actuator >= 0)
		{
			PeriphBindings* bindings = device->ActuatorBindings(pad_capture_actuator);

			if (bindings != nullptr)
			{
				if (pad_capture_gamepad)
				{
					bindings->gamepad = pad_captured_binding;
				}
				else
				{
					bindings->keyboard = pad_captured_binding;
				}

				device->SaveConfig(pad_capture_device);
			}
		}

		pad_capture_abort();
	}

	ImGui::SetNextWindowSize(ImVec2(780, 560), ImGuiCond_FirstUseEver);

	bool open = true;

	if (ImGui::Begin("Configure " APPNAME_A, &open))
	{
		static const char* tabs[(int)SettingsTab::Max] =
		{
			"General", "GCN Hardware", "Controllers", "Memory Cards"
		};

		const float footer = ImGui::GetFrameHeightWithSpacing();

		// The vertical strip of tabs
		ImGui::BeginChild("settings_tabs", ImVec2(160, -footer), true);

		for (int i = 0; i < (int)SettingsTab::Max; i++)
		{
			if (ImGui::Selectable(tabs[i], (int)settings_tab == i))
			{
				settings_tab = (SettingsTab)i;
			}
		}

		ImGui::EndChild();

		ImGui::SameLine();

		// The page
		ImGui::BeginChild("settings_page", ImVec2(0, -footer));

		switch (settings_tab)
		{
			case SettingsTab::General:     settings_page_general(); break;
			case SettingsTab::Hardware:    settings_page_hw(); break;
			case SettingsTab::Controllers: settings_page_devices(PERIPH_BUS_SI); break;
			case SettingsTab::MemoryCards: settings_page_devices(PERIPH_BUS_EXI); break;
		}

		ImGui::EndChild();

		ImGui::Separator();

		if (ImGui::Button("Close", ImVec2(80, 0)))
		{
			settings_open = false;
			pad_capture_abort();
		}
	}

	ImGui::End();

	if (!open)
	{
		settings_open = false;
		pad_capture_abort();
	}

	// The popups and the browsers of the window belong to the frame, not to the window: they are
	// drawn after it, so that they are not clipped by it.
	card_size_popup_window();
	settings_error_box();
	settings_poll_files();
}
