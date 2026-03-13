#pragma once
#include <format>
#include <string>
#include <string_view>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

enum class ELogVerbosity
{
	Verbose,
	Message,
	Display,
	Warning,
	Fatal
};

class Log
{
public:
	static constexpr std::string_view kDefaultTag = "Log";

	template <typename... Args>
	static void Print(ELogVerbosity verbosity, std::string_view tag, std::format_string<Args...> fmt, Args&&... args)
	{
		std::string formattedMsg = std::vformat(fmt.get(), std::make_format_args(args...));
		std::string finalMsg = std::format("[{}] {}\n", tag.empty() ? kDefaultTag : tag, formattedMsg);

#ifdef _WIN32
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		WORD colorAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Default (White)

		switch (verbosity)
		{
			case ELogVerbosity::Fatal:
				colorAttribute = FOREGROUND_RED | FOREGROUND_INTENSITY; // Red
				break;
			case ELogVerbosity::Warning:
				colorAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // Yellow
				break;
			case ELogVerbosity::Verbose:
				colorAttribute = FOREGROUND_INTENSITY; // Gray
				break;
			case ELogVerbosity::Display:
			case ELogVerbosity::Message:
			default:
				break; // White
		}

		SetConsoleTextAttribute(hConsole, colorAttribute);
		std::cout << finalMsg;
		OutputDebugStringA(finalMsg.c_str());
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset
#else
		// ANSI Escape Codes Ã³¸® (Windows ¿Ü ÇÃ·§Æû)
		const char* colorCode = "\033[0m"; // Reset
		switch (verbosity)
		{
			case ELogVerbosity::Fatal:       colorCode = "\033[1;31m"; break; // Red
			case ELogVerbosity::Warning:	 colorCode = "\033[1;33m"; break; // Yellow
			case ELogVerbosity::Verbose:	 colorCode = "\033[1;30m"; break; // Gray
			default: break; // White
		}
		std::fprintf(stderr, "%s%s\033[0m", colorCode, finalMsg.c_str());
#endif // _WIN32
	}
};

