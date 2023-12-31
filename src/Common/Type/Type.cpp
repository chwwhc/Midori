#include "Type.h"

#include <algorithm>

std::unordered_map<std::string, const MidoriType*> MidoriTypeUtil::s_types_by_name =
{
	// built-in types
	{"Int"s, new MidoriType(IntegerType())},
	{"Frac"s, new MidoriType(FractionType())},
	{"Text"s, new MidoriType(TextType())},
	{"Bool"s, new MidoriType(BoolType())},
	{"Unit"s, new MidoriType(UnitType())},
};

std::unordered_map<const MidoriType*, std::string> MidoriTypeUtil::s_names_by_type =
{
	// built-in types 
	{s_types_by_name["Int"s], "Int"s},
	{s_types_by_name["Frac"s], "Frac"s},
	{s_types_by_name["Text"s], "Text"s},
	{s_types_by_name["Bool"s], "Bool"s},
	{s_types_by_name["Unit"s], "Unit"s},
};

void MidoriTypeUtil::MidoriTypeUtilCleanUp()
{
	std::for_each(s_types_by_name.begin(), s_types_by_name.end(), [](const std::pair<std::string, const MidoriType*>& pair) -> void
		{
			delete pair.second;
		});
}

const MidoriType* MidoriTypeUtil::InsertType(const std::string& name, const MidoriType* type)
{
	s_types_by_name.emplace(name, std::move(type));
	s_names_by_type.emplace(type, name);

	return s_types_by_name[name];
}

const MidoriType* MidoriTypeUtil::InsertStructType(const std::string& name, std::vector<const MidoriType*>&& member_types, std::vector<std::string>&& member_names)
{
	const MidoriType* type = new MidoriType(StructType{ std::move(member_types), std::move(member_names), name });
	return InsertType(name, type);
}

const MidoriType* MidoriTypeUtil::InsertArrayType(const MidoriType* element_type)
{
	std::string array_type_name = "Array<"s + GetTypeName(element_type) + ">"s;

	std::unordered_map<std::string, const MidoriType*>::const_iterator find_result = s_types_by_name.find(array_type_name);
	if (find_result == s_types_by_name.end())
	{
		return InsertType(array_type_name, new MidoriType(ArrayType{ element_type }));
	}
	else
	{
		return s_types_by_name[array_type_name];
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

	std::unordered_map<std::string, const MidoriType*>::const_iterator find_result = s_types_by_name.find(function_type_name);
	if (find_result == s_types_by_name.end())
	{
		return InsertType(function_type_name, new MidoriType(FunctionType{ param_types, return_type, is_foreign }));
	}
	else
	{
		return s_types_by_name[function_type_name];
	}
}

const MidoriType* MidoriTypeUtil::GetType(const std::string& name)
{
	return s_types_by_name[name];
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

bool MidoriTypeUtil::IsNumericType(const MidoriType* type)
{
	return IsFractionType(type) || IsIntegerType(type);
}