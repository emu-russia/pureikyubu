#pragma once

#include <list>
#include <string>
#include <atomic>
#include <vector>
#include <string>
#include <fmt/format.h>

// file type (*.bin is not supported, and can be opened only by File->Open)
enum class SELECTOR_FILE
{
    Executable = 1,     // any GC executable (*.dol, *.elf)
    Dvd                 // any DVD image (*.gcm, *.iso)
};

// file entry
struct UserFile
{
    SELECTOR_FILE   type;       // see above (one of SELECTOR_FILE_*)
    size_t          size;       // file size
    std::wstring    id;         // GameID = DiskID + banner checksum
    std::wstring    name;       // file path and name
    std::wstring    title;      // alternate file name
    std::wstring    comment;    // some notes
    int             icon[2];    // banner/icon + same but highlighted
};

// selector columns
constexpr wchar_t SELECTOR_COLUMN_BANNER[]  = L"Icon";
constexpr wchar_t SELECTOR_COLUMN_TITLE[]   = L"Title";
constexpr wchar_t SELECTOR_COLUMN_SIZE[]    = L"Size";
constexpr wchar_t SELECTOR_COLUMN_GAMEID[]  = L"Game ID";
constexpr wchar_t SELECTOR_COLUMN_COMMENT[] = L"Comment";

// sort by ..
enum class SELECTOR_SORT
{
    Unsorted = 0,
    Default = 1,      // first by icon, then by title
    Filename,
    Title,
    Size,
    ID,
    Comment,
};

// selector API
void    CreateSelector();
void    CloseSelector();
void    SetSelectorIconSize(bool smallIcon);
bool    AddSelectorPath(std::wstring fullPath);            // FALSE, if path duplicated
void    ResizeSelector(int width, int height);
void    UpdateSelector();
int     SelectorGetSelected();
void    SelectorSetSelected(int item);
void    SelectorSetSelected(std::wstring_view filename);
void    SortSelector(SELECTOR_SORT sortBy);
void    DrawSelectorItem(LPDRAWITEMSTRUCT item);
void    NotifySelector(LPNMHDR pnmh);
void    ScrollSelector(int letter);
uint16_t* SjisToUnicode(TCHAR* sjisText, size_t* size, size_t* chars);

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
    SpinLock filesLock;

    std::atomic<bool> updateInProgress;

};

extern  UserSelector usel;
