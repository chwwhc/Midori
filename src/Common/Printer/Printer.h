#pragma once

#include <string_view>
#include <concepts>
#include <print>

namespace Printer
{
	enum class Color
	{
		RED,		// error
		GREEN,		// success
		YELLOW,		// warning
		BLUE,		// info
		MAGENTA,	// debug
		CYAN,		// trace
		WHITE,		// default
	};

	template<Color color = Color::WHITE>
	void Print(std::string_view message)
	{
		if constexpr (color == Color::RED)
		{
			std::printf("\033[31m");
		}
		else if constexpr (color == Color::GREEN)
		{
			std::printf("\033[32m");
		}
		else if constexpr (color == Color::YELLOW)
		{
			std::printf("\033[33m");
		}
		else if constexpr (color == Color::BLUE)
		{
			std::printf("\033[34m");
		}
		else if constexpr (color == Color::MAGENTA)
		{
			std::printf("\033[35m");
		}
		else if constexpr (color == Color::CYAN)
		{
			std::printf("\033[36m");
		}
		else if constexpr (color == Color::WHITE)
		{
			std::printf("\033[37m");
		}
		else
		{
			std::printf("\033[0m");
		}

		std::print("{}", message);
		std::printf("\033[0m");
	}
}