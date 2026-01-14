#pragma once
#include <string>
#include <typeinfo>

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
