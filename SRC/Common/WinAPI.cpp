#include "pch.h"

namespace Win
{
    std::wstring OpenDialog(std::wstring_view title, std::wstring_view filter, bool pick_folder)
    {
        PWSTR pszFilePath = nullptr;
        IFileOpenDialog* pFileOpen;
        DWORD options;

        /* Create the FileOpenDialog object. */
        auto hr = CoCreateInstance(CLSID_FileOpenDialog,
            NULL, CLSCTX_ALL,
            IID_IFileOpenDialog,
            (void**)&pFileOpen);
        
        if (SUCCEEDED(hr))
        {
            /* Get the dialog options. */
            pFileOpen->GetOptions(&options);

            pFileOpen->SetTitle(title.data());
            pFileOpen->SetDefaultExtension(filter.data());

            /* Allow the user to pick folders. */
            if (pick_folder)
            {
                options |= FOS_PICKFOLDERS;
            }

            pFileOpen->SetOptions(options);

            /* Show the Open dialog box. */
            hr = pFileOpen->Show(NULL);

            /* Get the file name from the dialog box. */
            if (SUCCEEDED(hr))
            {
                IShellItem* pItem;
                hr = pFileOpen->GetResult(&pItem);

                if (SUCCEEDED(hr))
                {                    
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    /* Display the file name to the user. */
                    if (SUCCEEDED(hr))
                    {
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }

        if (!pszFilePath)
        {
            return std::wstring();
        }
        else
        {
            return std::wstring(pszFilePath);
        }
    }
}
