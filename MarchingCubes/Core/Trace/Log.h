#pragma once
#include <cstdarg>
#include <cstdio>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32


class Log
{
public:
	static constexpr std::string_view kDefaultTag = "Log";

	static void Print(std::string_view tag, const char* fmt, ...)
	{
		char msg[1024]{};
		va_list ap;
		va_start(ap, fmt);
		std::vsnprintf(msg, sizeof(msg), fmt, ap);
		va_end(ap);

#ifdef _WIN32
		char buf[1200]{};
		if (!tag.empty())
		{
			std::snprintf(buf, sizeof(buf), "[%.*s] %s\n", (int)tag.size(), tag.data(), msg);
		}
		else
		{
			std::snprintf(buf, sizeof(buf), "%s\n", msg);
		}
		OutputDebugStringA(buf);
#else
		if (!tag.empty())
		{
			std::fprintf(stderr, "[%.*s] %s\n", (int)tag.size(), tag.data(), msg);
		}
		else
		{
			std::fprintf(stderr, "%s\n", msg);
		}
#endif // _WIN32
	}
};

