#pragma once

#include <string>
#include <variant>

class Object
{
public:
	constexpr inline std::string ToString() const
	{
		return std::visit([](auto&& arg) -> std::string
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, std::string>)
				{
					return '"' + arg + '"';
				}
			}, m_value);
	}

private:

public:

private:
	std::variant<std::string> m_value;
};
