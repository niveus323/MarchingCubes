#pragma once
#include <windows.h>
#include <commdlg.h>
#include <string>

namespace FileUtils
{
    inline std::string OpenFileDialog(const char* filter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0")
    {
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn))
        {
            return std::string(ofn.lpstrFile);
        }
        return "";
    }
}