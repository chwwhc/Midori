#pragma once

#include "Object.h"

class Value
{
public:

	constexpr Value(double value) : m_value(value) {}

	constexpr Value(Object* value) : m_value(value) {}

	/**
	* @brief Returns the value as a float.
	*/
	constexpr inline double GetFloat() const
	{
		return std::get<double>(m_value);
	}

	/**
	 * @brief Returns true if the value is a float.
	 */
	constexpr inline bool IsFloat() const
	{
		return std::holds_alternative<double>(m_value);
	}

	/**
	* @brief Returns the value as a object pointer.
	*/
	constexpr inline Object* GetObjectPointer() const
	{
		return std::get<Object*>(m_value);
	}

	/**
	 * @brief Returns true if the value is a object pointer.
	 */
	constexpr inline bool IsObjectPointer() const
	{
		return std::holds_alternative<Object*>(m_value);
	}

	/**
	 * @brief Returns a string representation of the value.
	 */
	constexpr inline std::string ToString() const
	{
		return std::visit([](auto&& arg) -> std::string
			{
				using Type = std::decay_t<decltype(arg)>;

				if constexpr (std::is_same_v<Type, double>)
				{
					return std::to_string(arg);
				}
				else if constexpr (std::is_same_v<Type, Object*>)
				{
					return arg->ToString();
				}
			}, m_value);
	}

private:
	std::variant<double, Object*> m_value;
};
