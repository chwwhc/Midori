#include "Type.h"

#include <algorithm>

std::unordered_map<std::string, MidoriType> MidoriTypeUtil::s_types_by_name =
{
	// built-in types
	{"Int"s, MidoriType(IntegerType())},
	{"Frac"s, MidoriType(FractionType())},
	{"Text"s, MidoriType(TextType())},
	{"Bool"s, MidoriType(BoolType())},
	{"Unit"s, MidoriType(UnitType())},
};

std::unordered_map<const MidoriType*, std::string> MidoriTypeUtil::s_names_by_type =
{
	// built-in types 
	{&s_types_by_name["Int"s], "Int"s},
	{&s_types_by_name["Frac"s], "Frac"s},
	{&s_types_by_name["Text"s], "Text"s},
	{&s_types_by_name["Bool"s], "Bool"s},
	{&s_types_by_name["Unit"s], "Unit"s},
};

const MidoriType* MidoriTypeUtil::InsertType(const std::string& name, MidoriType&& type)
{
	s_types_by_name.emplace(name, std::move(type));
	s_names_by_type.emplace(&s_types_by_name[name], name);

	return &s_types_by_name[name];
}

const MidoriType* MidoriTypeUtil::InsertUnionType(const std::string& name)
{
	return InsertType(name, MidoriType(UnionType{ {}, name }));
}

const MidoriType* MidoriTypeUtil::InsertStructType(const std::string& name, std::vector<const MidoriType*>&& member_types, std::vector<std::string>&& member_names)
{
	return InsertType(name, MidoriType(StructType{ std::move(member_types), std::move(member_names), name }));
}

const MidoriType* MidoriTypeUtil::InsertArrayType(const MidoriType* element_type)
{
	std::string array_type_name = "Array["s + GetTypeName(element_type) + "]"s;

	if (s_types_by_name.contains(array_type_name))
	{
		return &s_types_by_name[array_type_name];
	}
	else
	{
		return InsertType(array_type_name, MidoriType(ArrayType{ element_type }));
	}
}

const MidoriType* MidoriTypeUtil::InsertFunctionType(const std::vector<const MidoriType*>& param_types, const MidoriType* return_type, bool is_foreign)
{
	std::string function_type_name = is_foreign ? "FFI ("s : "("s;
	if (!param_types.empty())
	{
		std::for_each(param_types.begin(), param_types.end(), [&function_type_name](const MidoriType* param_type) -> void
			{
				function_type_name += GetTypeName(param_type) + ", "s;
			});
		function_type_name.pop_back();
		function_type_name.pop_back();
		function_type_name.push_back(')');
	}
	else
	{
		function_type_name.push_back(')');
	}
	function_type_name.append("->"s);
	function_type_name.append(GetTypeName(return_type));

	if (s_types_by_name.contains(function_type_name))
	{
		return &s_types_by_name[function_type_name];
	}
	else
	{
		return InsertType(function_type_name, MidoriType(FunctionType{ param_types, return_type, is_foreign }));
	}
}

const MidoriType* MidoriTypeUtil::GetType(const std::string& name)
{
	return &s_types_by_name[name];
}

const std::string& MidoriTypeUtil::GetTypeName(const MidoriType* type)
{
	return s_names_by_type[type];
}

bool MidoriTypeUtil::IsFractionType(const MidoriType* type)
{
	return std::holds_alternative<FractionType>(*type);
}

const FractionType& MidoriTypeUtil::GetFractionType(const MidoriType* type)
{
	return std::get<FractionType>(*type);
}

bool MidoriTypeUtil::IsIntegerType(const MidoriType* type)
{
	return std::holds_alternative<IntegerType>(*type);
}

const IntegerType& MidoriTypeUtil::GetIntegerType(const MidoriType* type)
{
	return std::get<IntegerType>(*type);
}

bool MidoriTypeUtil::IsTextType(const MidoriType* type)
{
	return std::holds_alternative<TextType>(*type);
}

const TextType& MidoriTypeUtil::GetTextType(const MidoriType* type)
{
	return std::get<TextType>(*type);
}

bool MidoriTypeUtil::IsBoolType(const MidoriType* type)
{
	return std::holds_alternative<BoolType>(*type);
}

const BoolType& MidoriTypeUtil::GetBoolType(const MidoriType* type)
{
	return std::get<BoolType>(*type);
}

bool MidoriTypeUtil::IsUnitType(const MidoriType* type)
{
	return std::holds_alternative<UnitType>(*type);
}

const UnitType& MidoriTypeUtil::GetUnitType(const MidoriType* type)
{
	return std::get<UnitType>(*type);
}

bool MidoriTypeUtil::IsArrayType(const MidoriType* type)
{
	return std::holds_alternative<ArrayType>(*type);
}

const ArrayType& MidoriTypeUtil::GetArrayType(const MidoriType* type)
{
	return std::get<ArrayType>(*type);
}

bool MidoriTypeUtil::IsFunctionType(const MidoriType* type)
{
	return std::holds_alternative<FunctionType>(*type);
}

const FunctionType& MidoriTypeUtil::GetFunctionType(const MidoriType* type)
{
	return std::get<FunctionType>(*type);
}

bool MidoriTypeUtil::IsStructType(const MidoriType* type)
{
	return std::holds_alternative<StructType>(*type);
}

const StructType& MidoriTypeUtil::GetStructType(const MidoriType* type)
{
	return std::get<StructType>(*type);
}

bool MidoriTypeUtil::IsUnionType(const MidoriType* type)
{
	return std::holds_alternative<UnionType>(*type);
}

const UnionType& MidoriTypeUtil::GetUnionType(const MidoriType* type)
{
	return std::get<UnionType>(*type);
}

bool MidoriTypeUtil::IsNumericType(const MidoriType* type)
{
	return IsFractionType(type) || IsIntegerType(type);
}