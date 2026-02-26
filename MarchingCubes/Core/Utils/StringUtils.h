#pragma once
#include <string>
#include <typeinfo>

namespace StringUtils
{
    inline std::string GetCleanClassName(const char* rawName)
    {
        std::string name = rawName;
        size_t pos = name.find("class ");
        if (pos != std::string::npos) {
            return name.substr(pos + 6);
        }

        pos = name.find("struct ");
        if (pos != std::string::npos) {
            return name.substr(pos + 7);
        }

        return name;
    }

    inline std::wstring ToWString(const std::string& str)
    {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    inline std::string ToString(const std::wstring& wstr)
    {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    inline std::wstring ToLowerCopy(const std::wstring& s)
    {
        std::wstring r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return r;
    }

}