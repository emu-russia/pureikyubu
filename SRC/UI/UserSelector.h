#pragma once

#include <list>
#include <string>
#include <atomic>
#include <vector>

// file type (*.bin is not supported, and can be opened only by File->Open)
enum class SELECTOR_FILE
{
    Executable = 1,     // any GC executable (*.dol, *.elf)
    Dvd                 // any DVD image (*.gcm, *.iso)
};

// file info limits
#define MAX_TITLE       0x100
#define MAX_COMMENT     0x100

// file entry
typedef struct UserFile
{
    SELECTOR_FILE   type;           // see above (one of SELECTOR_FILE_*)
    size_t  size;                   // file size
    TCHAR   id[0x10];               // GameID = DiskID + banner checksum
    TCHAR   name[2*MAX_PATH+2];     // file path and name
    TCHAR   title[MAX_TITLE];       // alternate file name
    TCHAR   comment[MAX_COMMENT];   // some notes
    int     icon[2];                // banner/icon + same but highlighted
} UserFile;

// selector columns
#define SELECTOR_COLUMN_BANNER  _T("Icon")
#define SELECTOR_COLUMN_TITLE   _T("Title")
#define SELECTOR_COLUMN_SIZE    _T("Size")
#define SELECTOR_COLUMN_GAMEID  _T("Game ID")
#define SELECTOR_COLUMN_COMMENT _T("Comment")

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
bool    AddSelectorPath(TCHAR *fullPath);            // FALSE, if path duplicated
void    ResizeSelector(int width, int height);
void    UpdateSelector();
int     SelectorGetSelected();
void    SelectorSetSelected(int item);
void    SelectorSetSelected(TCHAR *filename);
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
    std::vector<TCHAR *> paths;

    // file filter
    uint32_t    filter;             // every 8-bits masking extension : [DOL][ELF][GCM][GMP]

    // list of found files
    std::vector<UserFile*> files;
    SpinLock filesLock;

    std::atomic<bool> updateInProgress;

};

extern  UserSelector usel;
