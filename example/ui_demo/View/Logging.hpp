#pragma once

#include <iostream>

namespace example::ui_demo
{
inline void LogInfo(const char* message)
{
	std::clog << message << '\n';
}

template <typename... Args>
void LogInfo(const char* message, const Args&... args)
{
	(void)sizeof...(args);
	std::clog << message << '\n';
}

inline void LogError(const char* message)
{
	std::cerr << message << '\n';
}

template <typename... Args>
void LogError(const char* message, const Args&... args)
{
	(void)sizeof...(args);
	std::cerr << message << '\n';
}
} // namespace example::ui_demo
