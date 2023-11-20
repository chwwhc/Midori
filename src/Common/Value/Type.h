#pragma once

#include <variant>
#include <vector>
#include <memory>
#include <string>

struct FractionType {};
struct IntegerType {};
struct TextType {};
struct BoolType {};
struct UnitType {};
struct UndecidedType {};
struct ArrayType;
struct FunctionType;

using MidoriType = std::variant<FractionType, TextType, BoolType, UnitType, UndecidedType, ArrayType, FunctionType, IntegerType>;

struct ArrayType
{
	std::shared_ptr<MidoriType> m_element_type;
};

struct FunctionType
{
	std::vector<std::shared_ptr<MidoriType>> m_param_types;
	std::shared_ptr<MidoriType> m_return_type;
	bool m_is_native = false;
};

template<typename T>
inline bool operator==(const UndecidedType&, const T&) { return false; }

template<typename T>
inline bool operator!=(const UndecidedType&, const T&) { return true; }

inline bool operator==(const IntegerType&, const IntegerType&) { return true; }

inline bool operator==(const FractionType&, const FractionType&) { return true; }

inline bool operator==(const TextType&, const TextType&) { return true; }

inline bool operator==(const BoolType&, const BoolType&) { return true; }

inline bool operator==(const UnitType&, const UnitType&) { return true; }

inline bool operator!=(const IntegerType& lhs, const IntegerType& rhs) { return !(lhs == rhs); }

inline bool operator!=(const FractionType& lhs, const FractionType& rhs) { return !(lhs == rhs); }

inline bool operator!=(const TextType& lhs, const TextType& rhs) { return !(lhs == rhs); }

inline bool operator!=(const BoolType& lhs, const BoolType& rhs) { return !(lhs == rhs); }

inline bool operator!=(const UnitType& lhs, const UnitType& rhs) { return !(lhs == rhs); }

inline bool operator==(const ArrayType& lhs, const ArrayType& rhs) { return *lhs.m_element_type == *rhs.m_element_type; }

inline bool operator!=(const ArrayType& lhs, const ArrayType& rhs) { return !(lhs == rhs); }

inline bool operator==(const FunctionType& lhs, const FunctionType& rhs)
{
	if (lhs.m_param_types.size() != rhs.m_param_types.size())
	{
		return false;
	}
	for (size_t i = 0u; i < lhs.m_param_types.size(); i += 1u)
	{
		if (*lhs.m_param_types[i] != *rhs.m_param_types[i])
		{
			return false;
		}
	}
	return *lhs.m_return_type == *rhs.m_return_type;
}

inline bool operator!=(const FunctionType& lhs, const FunctionType& rhs) { return !(lhs == rhs); }

inline bool operator==(const MidoriType& lhs, const MidoriType& rhs)
{
	return std::visit([](const auto& a, const auto& b) -> bool
		{
			// If types mismatch, variants are not equal
			if constexpr (!std::is_same_v<decltype(a), decltype(b)>)
			{
				return false;
			}
			else
			{
				// Types match, compare values
				return a == b;
			}}, lhs, rhs);
}

inline bool operator!=(const MidoriType& lhs, const MidoriType& rhs) { return !(lhs == rhs); }

namespace MidoriTypeUtil
{
	inline bool IsFractionType(const MidoriType& type) { return std::holds_alternative<FractionType>(type); }

	inline const FractionType& GetFractionType(const MidoriType& type) { return std::get<FractionType>(type); }

	inline bool IsIntegerType(const MidoriType& type) { return std::holds_alternative<IntegerType>(type); }

	inline const IntegerType& GetIntegerType(const MidoriType& type) { return std::get<IntegerType>(type); }

	inline bool IsTextType(const MidoriType& type) { return std::holds_alternative<TextType>(type); }

	inline const TextType& GetTextType(const MidoriType& type) { return std::get<TextType>(type); }

	inline bool IsBoolType(const MidoriType& type) { return std::holds_alternative<BoolType>(type); }

	inline const BoolType& GetBoolType(const MidoriType& type) { return std::get<BoolType>(type); }

	inline bool IsUnitType(const MidoriType& type) { return std::holds_alternative<UnitType>(type); }

	inline const UnitType& GetUnitType(const MidoriType& type) { return std::get<UnitType>(type); }

	inline bool IsArrayType(const MidoriType& type) { return std::holds_alternative<ArrayType>(type); }

	inline const ArrayType& GetArrayType(const MidoriType& type) { return std::get<ArrayType>(type); }

	inline bool IsFunctionType(const MidoriType& type) { return std::holds_alternative<FunctionType>(type); }

	inline const FunctionType& GetFunctionType(const MidoriType& type) { return std::get<FunctionType>(type); }

	inline bool IsNumericType(const MidoriType& type) { return IsFractionType(type) || IsIntegerType(type); }

	inline std::string ToString(const MidoriType& type)
	{
		return std::visit([](const auto& a) -> std::string
			{
				using T = std::decay_t<decltype(a)>;
				if constexpr (std::is_same_v<T, FractionType>)
				{
					return "Fraction";
				}
				else if constexpr (std::is_same_v<T, IntegerType>)
				{
					return "Integer";
				}
				else if constexpr (std::is_same_v<T, TextType>)
				{
					return "Text";
				}
				else if constexpr (std::is_same_v<T, BoolType>)
				{
					return "Bool";
				}
				else if constexpr (std::is_same_v<T, UnitType>)
				{
					return "Unit";
				}
				else if constexpr (std::is_same_v<T, ArrayType>)
				{
					return "Array<" + ToString(*a.m_element_type) + ">";
				}
				else if constexpr (std::is_same_v<T, FunctionType>)
				{
					std::string result = "(";
					for (size_t i = 0u; i < a.m_param_types.size(); i += 1u)
					{
						result += ToString(*a.m_param_types[i]);
						if (i != a.m_param_types.size() - 1u)
						{
							result += ", ";
						}
					}
					result += ") -> " + ToString(*a.m_return_type);
					return result;
				}
				else if constexpr (std::is_same_v<T, UndecidedType>)
				{
					return "Undecided";
				}
				else
				{
					return "unknown";
				}
			}, type);
	}
}