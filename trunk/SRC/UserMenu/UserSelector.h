#ifndef __USER_SELECTOR_H__
#define __USER_SELECTOR_H__

// file type (*.bin is not supported, and can be opened only by File->Open)
enum SELECTOR_FILE
{
    SELECTOR_FILE_EXEC = 1,         // any GC executable (*.dol, *.elf)
    SELECTOR_FILE_DVD               // any DVD image (*.gcm, *.gmp)
};

// file info limits
#define MAX_TITLE       128         // 64 wasnt enough :(
#define MAX_COMMENT     128

// file entry
typedef struct UserFile
{
    s32     type;                   // see above (one of SELECTOR_FILE_*)
    s32     size;                   // file size
    char    id[8];                  // GameID = DiskID + banner checksum
    char    name[2*MAX_PATH+2];     // file path and name
    char    title[MAX_TITLE];       // alternate file name
    char    comment[MAX_COMMENT];   // some notes
    s32     icon[2];                // banner/icon + same but highlighted
} UserFile;

// selector columns
#define SELECTOR_COLUMN_BANNER  "Icon"
#define SELECTOR_COLUMN_TITLE   "Title"
#define SELECTOR_COLUMN_SIZE    "Size"
#define SELECTOR_COLUMN_GAMEID  "Game ID"
#define SELECTOR_COLUMN_COMMENT "Comment"

// sort by ..
enum SELECTOR_SORT
{
    SELECTOR_SORT_DEFAULT = 1,      // first by icon, then by title
    SELECTOR_SORT_FILENAME,
    SELECTOR_SORT_TITLE,
    SELECTOR_SORT_SIZE,
    SELECTOR_SORT_ID,
    SELECTOR_SORT_COMMENT
};

// selector API
void    CreateSelector();
void    CloseSelector();
void    SetSelectorIconSize(BOOL smallIcon);        // <-- funny "BOOL small" compiler bug is here
BOOL    AddSelectorPath(char *fullPath);            // FALSE, if path duplicated
void    ResizeSelector(s32 width, s32 height);
void    UpdateSelector();
s32     SelectorGetSelected();
void    SelectorSetSelected(s32 item);
void    SelectorSetSelected(char *filename);
void    SortSelector(s32 sortBy);
void    DrawSelectorItem(LPDRAWITEMSTRUCT item);
void    NotifySelector(LPNMHDR pnmh);
void    ScrollSelector(char letter);

// all important data is placed here
typedef struct UserSelector
{
    BOOL        active;             // 1, if enabled (under control of UserWindow)
    BOOL        opened;             // 1, if visible
    BOOL        smallIcons;         // show small icons
    s32         sortBy;             // sort rule (one of SELECTOR_SORT_*)
    s32         width;              // selector width
    s32         height;             // selector height

    HWND        hSelectorWindow;    // selector window handler
    HMENU       hFileMenu;          // popup file menu
    BOOL        compress;           // current file action (1:compress, 0:decompress)
    char        file1[256];         // first file for GCMCMPR
    char        file2[256];         // second file for GCMCMPR
    UserFile*   selected;           // first selected item (temporary for edit file dialog)

    // path list, where to search files.
    char**      paths;
    s32         pathnum;

    // file filter
    u32         filter;             // every 8-bits masking extension : [DOL][ELF][GCM][GMP]

    // we are using self-extended file list, so file count is
    // unlimited in theory (limited only by system resources).
    UserFile*   files;              // file list
    s32         filenum;            // file count
} UserSelector;

extern  UserSelector usel;

#endif  // __USER_SELECTOR_H__
