#include "AbstractSyntaxTreePrinter.h"

#include <iostream>

void PrintAbstractSyntaxTree::PrintWithIndentation(int depth, std::string text) const
{
	for (int i = 0; i < depth; i += 1)
	{
		std::cout << "  ";
	}

	std::cout << text << std::endl;
}

void PrintAbstractSyntaxTree::operator()(const Block& block, int depth) const
{
	PrintWithIndentation(depth, "Block {");
	for (const std::unique_ptr<Statement>& stmt : block.m_stmts)
	{
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 1); }, *stmt);
	}
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
	PrintWithIndentation(depth + 1, std::string("IsFixed: ") + (def.m_is_fixed ? "true" : "false"));
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
	PrintWithIndentation(depth + 1, "Condition Initializer: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *for_stmt.m_condition_intializer);
	if (for_stmt.m_condition.has_value())
	{
		PrintWithIndentation(depth + 1, "Condition: ");
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *for_stmt.m_condition.value());
	}
	PrintWithIndentation(depth + 1, "Body: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *for_stmt.m_body);
	if (for_stmt.m_condition_incrementer.has_value())
	{
		PrintWithIndentation(depth + 1, "Condition Incrementer: ");
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
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Import& import, int depth) const
{
	PrintWithIndentation(depth, "Import {");
	PrintWithIndentation(depth + 1, "Path: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *import.m_path);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Namespace& namespace_stmt, int depth) const
{
	PrintWithIndentation(depth, "Namespace {");
	PrintWithIndentation(depth + 1, "Name: " + namespace_stmt.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "Body: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *namespace_stmt.m_stmts);
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

void PrintAbstractSyntaxTree::operator()(const Logical& logical, int depth) const
{
	PrintWithIndentation(depth, "Logical {");
	PrintWithIndentation(depth + 1, "Operator: " + logical.m_op.m_lexeme);
	PrintWithIndentation(depth + 1, "Left: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *logical.m_left);
	PrintWithIndentation(depth + 1, "Right: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *logical.m_right);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Group& group, int depth) const
{
	PrintWithIndentation(depth, "Group {");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 1); }, *group.m_expr_in);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Unary& unary, int depth) const
{
	PrintWithIndentation(depth, "Unary {");
	PrintWithIndentation(depth + 1, "Operator: " + unary.m_op.m_lexeme);
	PrintWithIndentation(depth + 1, "Right: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *unary.m_right);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Call& call, int depth) const
{
	PrintWithIndentation(depth, "Call {");
	PrintWithIndentation(depth + 1, "Callee: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *call.m_callee);
	PrintWithIndentation(depth + 1, "Args: ");
	for (const std::unique_ptr<Expression>& arg : call.m_arguments)
	{
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *arg);
	}
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Get& get, int depth) const
{
	PrintWithIndentation(depth, "Get {");
	PrintWithIndentation(depth + 1, "Object: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *get.m_object);
	PrintWithIndentation(depth + 1, "Name: " + get.m_name.m_lexeme);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Set& set, int depth) const
{
	PrintWithIndentation(depth, "Set {");
	PrintWithIndentation(depth + 1, "Object: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *set.m_object);
	PrintWithIndentation(depth + 1, "Name: " + set.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *set.m_value);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Variable& variable, int depth) const
{
	PrintWithIndentation(depth, "Variable {");
	PrintWithIndentation(depth + 1, "Name: " + variable.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "VariableSemantic: ");
	std::visit([depth, this]([[maybe_unused]] auto&& arg)
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, VariableSemantic::Local> || std::is_same_v<T, VariableSemantic::Cell>)
			{
				PrintWithIndentation(depth + 2, "Local");
				PrintWithIndentation(depth + 2, "Index: " + std::to_string(arg.m_index));
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Global>)
			{
				PrintWithIndentation(depth + 2, "Global");
			}
		}, variable.m_semantic);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Assign& assign, int depth) const
{
	PrintWithIndentation(depth, "Assign {");
	PrintWithIndentation(depth + 1, "Name: " + assign.m_name.m_lexeme);
	PrintWithIndentation(depth + 1, "Value: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *assign.m_value);
	PrintWithIndentation(depth + 1, "VariableSemantic: ");
	std::visit([depth, this]([[maybe_unused]] auto&& arg)
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, VariableSemantic::Local> || std::is_same_v<T, VariableSemantic::Cell>)
			{
				PrintWithIndentation(depth + 2, "Local");
				PrintWithIndentation(depth + 2, "Index: " + std::to_string(arg.m_index));
			}
			else if constexpr (std::is_same_v<T, VariableSemantic::Global>)
			{
				PrintWithIndentation(depth + 2, "Global");
			}
		}, assign.m_semantic);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const String& string, int depth) const
{
	PrintWithIndentation(depth, "String {");
	PrintWithIndentation(depth + 1, "Value: \"" + string.m_token.m_lexeme + "\"");
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Bool& bool_val, int depth) const
{
	PrintWithIndentation(depth, "Bool {");
	PrintWithIndentation(depth + 1, "Value: " + bool_val.m_token.m_lexeme);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Number& number, int depth) const
{
	PrintWithIndentation(depth, "Number {");
	PrintWithIndentation(depth + 1, "Value: " + number.m_token.m_lexeme);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Unit&, int depth) const
{
	PrintWithIndentation(depth, "#");
}

void PrintAbstractSyntaxTree::operator()(const Function& function, int depth) const
{
	PrintWithIndentation(depth, "Function {");
	PrintWithIndentation(depth + 1, "Params: ");
	for (const Token& param : function.m_params)
	{
		PrintWithIndentation(depth + 2, param.m_lexeme);
	}
	PrintWithIndentation(depth + 1, "Body: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *function.m_body);
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const Array& array, int depth) const
{
	PrintWithIndentation(depth, "Array {");
	PrintWithIndentation(depth + 1, "Elements: ");
	for (const std::unique_ptr<Expression>& element : array.m_elems)
	{
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *element);
	}
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::operator()(const ArrayGet& array_get, int depth) const
{
	PrintWithIndentation(depth, "ArrayGet {");
	PrintWithIndentation(depth + 1, "Array: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *array_get.m_arr_var);
	PrintWithIndentation(depth + 1, "Index: ");
	for (const std::unique_ptr<Expression>& index : array_get.m_indices)
	{
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *index);
	}
	PrintWithIndentation(depth, "}");
}

void PrintAbstractSyntaxTree::PrintAbstractSyntaxTree::operator()(const ArraySet& array_set, int depth) const
{
	PrintWithIndentation(depth, "ArraySet {");
	PrintWithIndentation(depth + 1, "Array: ");
	std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *array_set.m_arr_var);
	PrintWithIndentation(depth + 1, "Index: ");
	for (const std::unique_ptr<Expression>& index : array_set.m_indices)
	{
		std::visit([depth, this](auto&& arg) { (*this)(arg, depth + 2); }, *index);
	}
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
