#ifdef DEBUG
#include "AbstractSyntaxTreePrinter.h"
#include "Common\Printer\Printer.h"

#include <algorithm>
#include <ranges>

void PrintAbstractSyntaxTree::PrintWithIndentation(int depth, std::string_view text) const
{
	Printer::Print(std::string(depth, ' '));
	Printer::Print(text);
	Printer::Print("\n");
}

void PrintAbstractSyntaxTree::PrintVariableSemantic(int depth, const VariableSemantic::Tag& tag) const
{
	std::visit
	(
		[depth, this](auto&& arg) -> void
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, VariableSemantic::Local>)
			{
				PrintWithIndentation(depth, "Local");
				PrintWithIndentation(depth, "Index: " + std::to_string(arg.m_index));
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Global>)
			{
				PrintWithIndentation(depth, "Global");
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Cell>)
			{
				PrintWithIndentation(depth, "Cell");
				PrintWithIndentation(depth, "Index: " + std::to_string(arg.m_index));
			}
		}, tag
	);
}

void PrintAbstractSyntaxTree::operator()(const Block& block, int depth) const
{
	PrintWithIndentation(depth, "Block {");
	std::ranges::for_each(block.m_stmts, [depth, this](const std::unique_ptr<MidoriStatement>& stmt)
		{
			std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 1); }, *stmt);
		});
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Simple& simple, int depth) const
{
	PrintWithIndentation(depth, "Simple {");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 1); }, *simple.m_expr);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Define& def, int depth) const
{
	PrintWithIndentation(depth, "Define {");
	PrintWithIndentation(depth + 1, "Name: " + def.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *def.m_value);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const If& if_stmt, int depth) const
{
	PrintWithIndentation(depth, "If {");
	PrintWithIndentation(depth + 1, "Condition: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *if_stmt.m_condition);
	PrintWithIndentation(depth + 1, "TrueBranch: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *if_stmt.m_true_branch);
	if (if_stmt.m_else_branch.has_value())
	{
		PrintWithIndentation(depth + 1, "ElseBranch: ");
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *if_stmt.m_else_branch.value());
	}
	PrintWithIndentation(depth + 1, "}");
}

void PrintAbstractSyntaxTree::operator()(const While& while_stmt, int depth) const
{
	PrintWithIndentation(depth, "While {");
	PrintWithIndentation(depth + 1, "Condition: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *while_stmt.m_condition);
	PrintWithIndentation(depth + 1, "Body: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *while_stmt.m_body);
	PrintWithIndentation(depth + 1, "}");
}

void PrintAbstractSyntaxTree::operator()(const For& for_stmt, int depth) const
{
	PrintWithIndentation(depth, "For {");
	if (for_stmt.m_condition_intializer.has_value())
	{
		PrintWithIndentation(depth + 1, "ConditionInitializer: ");
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *for_stmt.m_condition_intializer.value());
	}
	if (for_stmt.m_condition.has_value())
	{
		PrintWithIndentation(depth + 1, "Condition: ");
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *for_stmt.m_condition.value());
	}
	PrintWithIndentation(depth + 1, "Body: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *for_stmt.m_body);
	if (for_stmt.m_condition_incrementer.has_value())
	{
		PrintWithIndentation(depth + 1, "ConditionIncrementer: ");
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *for_stmt.m_condition_incrementer.value());
	}
	PrintWithIndentation(depth + 1, "}");
}

void PrintAbstractSyntaxTree::operator()(const Break&, int depth) const
{
	PrintWithIndentation(depth, "Break");
}

void PrintAbstractSyntaxTree::operator()(const Continue&, int depth) const
{
	PrintWithIndentation(depth, "Continue");
}

void PrintAbstractSyntaxTree::operator()(const Return& return_stmt, int depth) const
{
	PrintWithIndentation(depth, "Return {");
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *return_stmt.m_value);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Foreign& foreign, int depth) const
{
	PrintWithIndentation(depth, "ForeignFunctionInterface {");
	PrintWithIndentation(depth + 1, "Name: " + foreign.m_function_name.m_lexeme);
	PrintWithIndentation(depth + 1, "ForeignName: " + foreign.m_foreign_name);
	PrintWithIndentation(depth + 1, "Type: " + MidoriTypeUtil::GetTypeName(foreign.m_type));
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Struct& struct_stmt, int depth) const
{
	PrintWithIndentation(depth, "Struct {");
	PrintWithIndentation(depth + 1, "Name: " + struct_stmt.m_name.m_lexeme);
	const StructType& struct_type = MidoriTypeUtil::GetStructType(struct_stmt.m_self_type);
	for (size_t i = 0u; i < struct_type.m_member_names.size(); i += 1u)
	{
		PrintWithIndentation(depth + 1, "MemberName: ");
		PrintWithIndentation(depth + 2, struct_type.m_member_names[i]);
		PrintWithIndentation(depth + 1, "MemberType: ");
		PrintWithIndentation(depth + 2, MidoriTypeUtil::GetTypeName(struct_type.m_member_types[i]));
	}
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Union& union_stmt, int depth) const
{
	PrintWithIndentation(depth, "Union {");
	PrintWithIndentation(depth + 1, "Name: " + union_stmt.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "Constructors: ");

	const UnionType& union_type = MidoriTypeUtil::GetUnionType(union_stmt.m_self_type);

	for (const auto& [name, info] : union_type.m_member_info)
	{
		std::string constructor = name;
		constructor.push_back('(');
		for (size_t i = 0u; i < info.m_member_types.size(); i += 1u)
		{
			constructor.append(MidoriTypeUtil::GetTypeName(info.m_member_types[i]));
			if (i != info.m_member_types.size() - 1u)
			{
				constructor.append(", "s);
			}
		}
		constructor.push_back(')');
		PrintWithIndentation(depth + 1, constructor);
	}
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Switch& switch_stmt, int depth) const
{
	PrintWithIndentation(depth, "Switch {");
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *switch_stmt.m_arg_expr);
	PrintWithIndentation(depth + 1, "Cases: ");
	std::ranges::for_each(switch_stmt.m_cases, [depth, this](const Switch::Case& switch_case)
		{
			std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *Switch::GetCaseStatement(switch_case));
		});
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Namespace& namespace_stmt, int depth) const
{
	PrintWithIndentation(depth, "Namespace {");
	PrintWithIndentation(depth + 1, "Name: " + namespace_stmt.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "Body: ");
	std::ranges::for_each(namespace_stmt.m_stmts, [depth, this](const std::unique_ptr<MidoriStatement>& stmt)
		{
			std::visit([depth, this](auto&& arg)
				{
					(*this)(arg, depth + 1);
				}, *stmt);
		});
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const As& as, int depth) const
{
	PrintWithIndentation(depth, "As {");
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *as.m_expr);
	PrintWithIndentation(depth + 1, "Type: ");
	PrintWithIndentation(depth + 2, MidoriTypeUtil::GetTypeName(as.m_target_type));
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Binary& binary, int depth) const
{
	PrintWithIndentation(depth, "Binary {");
	PrintWithIndentation(depth + 1, "Operator: " + binary.m_op.m_lexeme);
	PrintWithIndentation(depth + 1, "Left: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *binary.m_left);
	PrintWithIndentation(depth + 1, "Right: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *binary.m_right);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Group& group, int depth) const
{
	PrintWithIndentation(depth, "Group {");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 1); }, *group.m_expr_in);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const UnaryPrefix& unary, int depth) const
{
	PrintWithIndentation(depth, "UnaryPrefix {");
	PrintWithIndentation(depth + 1, "Operator: " + unary.m_op.m_lexeme);
	PrintWithIndentation(depth + 1, "Operand: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *unary.m_expr);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const UnarySuffix& unary, int depth) const
{
	PrintWithIndentation(depth, "UnarySuffix {");
	PrintWithIndentation(depth + 1, "Operator: " + unary.m_op.m_lexeme);
	PrintWithIndentation(depth + 1, "Operand: ");
	std::visit([depth, this](auto&& arg)
		{
			(*this)(arg, depth + 2);
		}, *unary.m_expr);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Call& call, int depth) const
{
	PrintWithIndentation(depth, "Call {");
	PrintWithIndentation(depth + 1, "Callee: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *call.m_callee);
	PrintWithIndentation(depth + 1, "Args: ");
	std::ranges::for_each(call.m_arguments, [depth, this](const std::unique_ptr<MidoriExpression>& arg)
		{
			std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *arg);
		});
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Get& get, int depth) const
{
	PrintWithIndentation(depth, "Get {");
	PrintWithIndentation(depth + 1, "MidoriStruct: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *get.m_struct);
	PrintWithIndentation(depth + 1, "Name: " + get.m_member_name.m_lexeme);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Set& set, int depth) const
{
	PrintWithIndentation(depth, "Set {");
	PrintWithIndentation(depth + 1, "MidoriStruct: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *set.m_struct);
	PrintWithIndentation(depth + 1, "Name: " + set.m_member_name.m_lexeme);
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *set.m_value);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Variable& variable, int depth) const
{
	PrintWithIndentation(depth, "Variable {");
	PrintWithIndentation(depth + 1, "Name: " + variable.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "VariableSemantic: ");
	PrintVariableSemantic(depth, variable.m_semantic_tag);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Bind& bind, int depth) const
{
	PrintWithIndentation(depth, "Bind {");
	PrintWithIndentation(depth + 1, "Name: " + bind.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *bind.m_value);
	PrintWithIndentation(depth + 1, "VariableSemantic: ");
	PrintVariableSemantic(depth, bind.m_semantic_tag);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const TextLiteral& text, int depth) const
{
	PrintWithIndentation(depth, "Text {");
	PrintWithIndentation(depth + 1, "Value: \"" + text.m_token.m_lexeme + "\"");
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const BoolLiteral& bool_val, int depth) const
{
	PrintWithIndentation(depth, "Bool {");
	PrintWithIndentation(depth + 1, "Value: " + bool_val.m_token.m_lexeme);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const FractionLiteral& fraction, int depth) const
{
	PrintWithIndentation(depth, "Fraction {");
	PrintWithIndentation(depth + 1, "Value: " + fraction.m_token.m_lexeme);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const IntegerLiteral& integer, int depth) const
{
	PrintWithIndentation(depth, "Integer {");
	PrintWithIndentation(depth + 1, "Value: " + integer.m_token.m_lexeme);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const UnitLiteral&, int depth) const
{
	PrintWithIndentation(depth, "()");
}

void PrintAbstractSyntaxTree::operator()(const Closure& closure, int depth) const
{
	PrintWithIndentation(depth, "Closure {");
	PrintWithIndentation(depth + 1, "Params: ");
	std::ranges::for_each(closure.m_params, [depth, this](const Token& param)
		{
			PrintWithIndentation(depth + 2, param.m_lexeme);
		});
	PrintWithIndentation(depth + 1, "ParamTypes: ");
	std::ranges::for_each(closure.m_param_types, [depth, this](const MidoriType* param_type)
		{
			PrintWithIndentation(depth + 2, MidoriTypeUtil::GetTypeName(param_type));
		});
	PrintWithIndentation(depth + 1, "Body: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *closure.m_body);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Construct& construct, int depth) const
{
	PrintWithIndentation(depth, "Construct {");
	PrintWithIndentation(depth + 1, "Type: " + MidoriTypeUtil::GetTypeName(construct.m_return_type));
	PrintWithIndentation(depth + 1, "Params: ");
	std::ranges::for_each(construct.m_params, [depth, this](const std::unique_ptr<MidoriExpression>& expr)
		{
			std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *expr);
		});
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Array& array, int depth) const
{
	PrintWithIndentation(depth, "Array {");
	PrintWithIndentation(depth + 1, "Elements: ");
	std::ranges::for_each(array.m_elems, [depth, this](const std::unique_ptr<MidoriExpression>& element)
		{
			std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *element);
		});
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const ArrayGet& array_get, int depth) const
{
	PrintWithIndentation(depth, "ArrayGet {");
	PrintWithIndentation(depth + 1, "Array: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *array_get.m_arr_var);
	PrintWithIndentation(depth + 1, "Index: ");
	std::ranges::for_each(array_get.m_indices, [depth, this](const std::unique_ptr<MidoriExpression>& index)
		{
			std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *index);
		});
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::PrintAbstractSyntaxTree::operator()(const ArraySet& array_set, int depth) const
{
	PrintWithIndentation(depth, "ArraySet {");
	PrintWithIndentation(depth + 1, "Array: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *array_set.m_arr_var);
	PrintWithIndentation(depth + 1, "Index: ");
	std::ranges::for_each(array_set.m_indices, [depth, this](const std::unique_ptr<MidoriExpression>& index)
		{
			std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *index);
		});
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *array_set.m_value);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::PrintAbstractSyntaxTree::operator()(const Ternary& ternary, int depth) const
{
	PrintWithIndentation(depth, "Ternary {");
	PrintWithIndentation(depth + 1, "Condition: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *ternary.m_condition);
	PrintWithIndentation(depth + 1, "Then: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *ternary.m_true_branch);
	PrintWithIndentation(depth + 1, "Else: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *ternary.m_else_branch);
	PrintWithIndentation(depth, "}");
}
#endif