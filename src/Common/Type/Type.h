#pragma once

#include <variant>
#include <vector>
#include <string>
#include <unordered_map>

using namespace std::string_literals;

struct FractionType {};
struct IntegerType {};
struct TextType {};
struct BoolType {};
struct UnitType {};
struct ArrayType;
struct FunctionType;
struct StructType;

using MidoriType = std::variant<FractionType, TextType, BoolType, UnitType, ArrayType, FunctionType, IntegerType, StructType>;

struct ArrayType
{
	const MidoriType* m_element_type;
};

struct FunctionType
{
	std::vector<const MidoriType*> m_param_types;
	const MidoriType* m_return_type;
	bool m_is_foreign = false;
};

struct StructType
{
	using MemberTypeInfo = std::pair<int, const MidoriType*>;
	using MemberTypeTable = std::unordered_map<std::string, MemberTypeInfo>;

	std::string m_name;
	MemberTypeTable m_member_types;
};

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

inline bool operator==(const StructType& lhs, const StructType& rhs) { return lhs.m_name == rhs.m_name; }

inline bool operator!=(const StructType& lhs, const StructType& rhs) { return !(lhs == rhs); }

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

class MidoriTypeUtil
{
private:

	static std::unordered_map<std::string, const MidoriType*> s_types_by_name;

	static std::unordered_map<const MidoriType*, std::string> s_names_by_type;

	static const MidoriType* InsertType(const std::string& name, const MidoriType* type);

public:

	static void MidoriTypeUtilCleanUp();

	static const MidoriType* InsertStructType(const std::string& name, StructType::MemberTypeTable&& member_table);

	static const MidoriType* InsertArrayType(const MidoriType* element_type);

	static const MidoriType* InsertFunctionType(const std::vector<const MidoriType*>& param_types, const MidoriType* return_type, bool is_foreign = false);

	static const MidoriType* GetType(const std::string& name);

	static const std::string& GetTypeName(const MidoriType* type);

	static bool IsFractionType(const MidoriType* type);

	static const FractionType& GetFractionType(const MidoriType* type);

	static bool IsIntegerType(const MidoriType* type);

	static const IntegerType& GetIntegerType(const MidoriType* type);

	static bool IsTextType(const MidoriType* type);

	static const TextType& GetTextType(const MidoriType* type);

	static bool IsBoolType(const MidoriType* type);

	static const BoolType& GetBoolType(const MidoriType* type);

	static bool IsUnitType(const MidoriType* type);

	static const UnitType& GetUnitType(const MidoriType* type);

	static bool IsArrayType(const MidoriType* type);

	static const ArrayType& GetArrayType(const MidoriType* type);

	static bool IsFunctionType(const MidoriType* type);

	static const FunctionType& GetFunctionType(const MidoriType* type);

	static bool IsStructType(const MidoriType* type);

	static const StructType& GetStructType(const MidoriType* type);

	static bool IsNumericType(const MidoriType* type);
};