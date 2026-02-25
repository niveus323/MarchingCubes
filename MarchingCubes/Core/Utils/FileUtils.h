#pragma once
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace FileUtils
{
    inline std::string OpenFileDialog(const char* initialDir = nullptr, const char* filter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0")
    {
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrInitialDir = initialDir;
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn))
        {
            return std::string(ofn.lpstrFile);
        }
        return "";
    }

    class FileDialogs
    {
    public:
        static std::string OpenFile(const char* filterName, const char* filterSpec);
        static std::string SaveFile(const char* filterName, const char* filterSpec);
    };

    bool DuplicateFile(const std::filesystem::path& source, const std::filesystem::path& dest, bool overwrite = true);
    bool WriteJSON(const std::filesystem::path& path, const nlohmann::json& data, int indent = 4);
    bool ReadJSON(const std::filesystem::path& path, nlohmann::json& outData);

    static std::optional<std::string> jopt_str(const nlohmann::json& j, const char* k) {
        auto it = j.find(k); if (it != j.end() && it->is_string()) return it->get<std::string>(); return std::nullopt;
    }
    static std::optional<bool> jopt_bool(const nlohmann::json& j, const char* k) {
        auto it = j.find(k); if (it != j.end() && it->is_boolean()) return it->get<bool>(); return std::nullopt;
    }
    static std::optional<int> jopt_int(const nlohmann::json& j, const char* k) {
        auto it = j.find(k); if (it != j.end() && it->is_number_integer()) return it->get<int>(); return std::nullopt;
    }

    class BinaryFile
    {
    public:
        struct DataChunk
        {
            const void* ptr = nullptr;
            size_t size = 0;
        };

        static bool WriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& data);
        static bool WriteFile(const std::filesystem::path& path, const std::vector<DataChunk>& chunks);
        static bool Readfile(const std::filesystem::path& path, std::vector<uint8_t>& outData);
    };
}