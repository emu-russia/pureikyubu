#pragma once

/* File type (*.bin is not supported, and can be opened only by File->Open) */
enum class SELECTOR_FILE
{
    Executable = 1,     /* any GC executable (*.dol, *.elf) */
    Dvd                 /* any DVD image (*.gcm, *.iso)     */
};

/* File info limits */
constexpr int MAX_TITLE     = 0x100;
constexpr int MAX_COMMENT   = 0x100;

/* File entry */
struct UserFile
{
    SELECTOR_FILE   type;       /* See above (one of SELECTOR_FILE_*)   */
    size_t          size;       /* File size                            */
    std::wstring    id;         /* GameID = DiskID + banner checksum    */
    std::wstring    name;       /* File path and name                   */
    wchar_t   title[MAX_TITLE];       // alternate file name
    wchar_t   comment[MAX_COMMENT];   // some notes
    int             icon[2];    /* Banner/icon + same but highlighted   */
};

/* Selector columns */
constexpr auto SELECTOR_COLUMN_BANNER  = L"Icon";
constexpr auto SELECTOR_COLUMN_TITLE   = L"Title";
constexpr auto SELECTOR_COLUMN_SIZE    = L"Size";
constexpr auto SELECTOR_COLUMN_GAMEID  = L"Game ID";
constexpr auto SELECTOR_COLUMN_COMMENT = L"Comment";

/* Sort by ... */
enum class SELECTOR_SORT
{
    Unsorted = 0,
    Default = 1,      /* First by icon, then by title */
    Filename,
    Title,
    Size,
    ID,
    Comment,
};

/* Selector API */
void CreateSelector();
void CloseSelector();
void SetSelectorIconSize(bool smallIcon);
bool AddSelectorPath(const std::wstring & fullPath);            // FALSE, if path duplicated
void ResizeSelector(int width, int height);
void UpdateSelector();
int  SelectorGetSelected();
void SelectorSetSelected(size_t item);
void SelectorSetSelected(const std::wstring & filename);
void SortSelector(SELECTOR_SORT sortBy);
void DrawSelectorItem(LPDRAWITEMSTRUCT item);
void NotifySelector(LPNMHDR pnmh);
void ScrollSelector(int letter);
uint16_t* SjisToUnicode(wchar_t* sjisText, size_t* size, size_t* chars);

// all important data is placed here
class UserSelector
{
public:
    bool        active;             // 1, if enabled (under control of UserWindow)
    bool        opened;             // 1, if visible
    bool        smallIcons;         // show small icons
    SELECTOR_SORT   sortBy;         // sort rule (one of SELECTOR_SORT_*)
    int         width;              // selector width
    int         height;             // selector height

    HWND        hSelectorWindow;    // selector window handler
    HMENU       hFileMenu;          // popup file menu

    // path list, where to search files.
    std::vector<std::wstring> paths;

    // file filter
    uint32_t    filter;             // every 8-bits masking extension : [DOL][ELF][GCM][GMP]

    // list of found files
    std::vector<std::unique_ptr<UserFile>> files;

    std::atomic<bool> updateInProgress;

};

extern  UserSelector usel;
