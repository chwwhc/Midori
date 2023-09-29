#pragma once

#include "Compiler/AbstractSyntaxTree/Expression.h"
#include "Compiler/AbstractSyntaxTree/Statement.h"

#include <iostream>

struct AbstractSyntaxTreePrinter
{
	void PrintWithIndentation(int depth, std::string text) const 
	{
		for (int i = 0; i < depth; i += 1) 
		{
			std::cout << "\t";  
		}

		std::cout << text << std::endl;
	}

	void operator()(const Block& block, int depth = 0) const
	{
		PrintWithIndentation(depth, "Block {");
		for (const std::unique_ptr<Statement>& stmt : block.m_stmts)
		{
			std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 1); }, *stmt);
		}
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Simple& simple, int depth = 0) const
	{
		PrintWithIndentation(depth, "Simple {");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 1); }, *simple.m_expr);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Print& print, int depth = 0) const
	{
		PrintWithIndentation(depth, "Print {");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 1); }, *print.m_expr);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Var& var, int depth = 0) const
	{
		PrintWithIndentation(depth, "Var {");
		for (const Var::VarContext& var_context : var.m_var_inits)
		{
			PrintWithIndentation(depth + 1, "VarContext {");
			PrintWithIndentation(depth + 1, "Name: " + var_context.m_name.m_lexeme);
			PrintWithIndentation(depth + 1, std::string("IsFixed: ") + (var_context.m_is_fixed ? "true" : "false"));
			PrintWithIndentation(depth + 1, "Value: ");
			if (var_context.m_value != nullptr)
			{
				std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *var_context.m_value);
			}
			else
			{
				PrintWithIndentation(depth + 2, "nil");
			}
			PrintWithIndentation(depth + 1, "}");
		}
		PrintWithIndentation(depth, "}");
	}

	void operator()(const If& if_stmt, int depth = 0) const
	{
		PrintWithIndentation(depth, "If {");
		PrintWithIndentation(depth + 1, "Condition: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *if_stmt.m_condition);
		PrintWithIndentation(depth + 1, "TrueBranch: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *if_stmt.m_true_branch);
		PrintWithIndentation(depth + 1, "ElseBranch: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *if_stmt.m_else_branch);
		PrintWithIndentation(depth + 1, "}");
	}

	void operator()(const While& while_stmt, int depth = 0) const
	{
		PrintWithIndentation(depth, "While {");
		PrintWithIndentation(depth + 1, "Condition: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *while_stmt.m_condition);
		PrintWithIndentation(depth + 1, "Body: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *while_stmt.m_body);
		PrintWithIndentation(depth + 1, "}");
	}

	void operator()(const Break&, int depth = 0) const
	{
		PrintWithIndentation(depth, "Break");
	}

	void operator()(const Continue&, int depth = 0) const
	{
		PrintWithIndentation(depth, "Continue");
	}

	void operator()(const Function& function, int depth = 0) const
	{
		PrintWithIndentation(depth, "Function {");
		PrintWithIndentation(depth + 1, "Name: " + function.m_name.m_lexeme);
		PrintWithIndentation(depth + 1, std::string("IsFixed: ") + (function.m_is_fixed ? "true" : "false"));
		PrintWithIndentation(depth + 1, std::string("IsSignature: ") + (function.m_is_sig ? "true" : "false"));
		PrintWithIndentation(depth + 1, "Params: ");
		for (const Token& param : function.m_params)
		{
			PrintWithIndentation(depth + 2, param.m_lexeme);
		}
		PrintWithIndentation(depth + 1, "Body: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *function.m_body);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Return& return_stmt, int depth = 0) const
	{
		PrintWithIndentation(depth, "Return {");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 1); }, *return_stmt.m_value);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Import& import, int depth = 0) const
	{
		PrintWithIndentation(depth, "Import {");
		PrintWithIndentation(depth + 1, "Path: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *import.m_path);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Class& class_, int depth = 0) const
	{
		PrintWithIndentation(depth, "Class {");
		PrintWithIndentation(depth + 1, "Name: " + class_.m_name.m_lexeme);
		PrintWithIndentation(depth + 1, std::string("IsFixed: ") + (class_.m_is_fixed ? "true" : "false"));
		PrintWithIndentation(depth + 1, "Superclass: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *class_.m_superclass);
		PrintWithIndentation(depth + 1, "Methods: ");
		for (const std::unique_ptr<Statement>& method : class_.m_methods)
		{
			std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *method);
		}
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Namespace& namespace_, int depth = 0) const
	{
		PrintWithIndentation(depth, "Namespace {");
		PrintWithIndentation(depth + 1, "Name: " + namespace_.m_name.m_lexeme);
		PrintWithIndentation(depth + 1, "IsFixed: " + namespace_.m_is_fixed ? "true" : "false");
		PrintWithIndentation(depth + 1, "Body: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *namespace_.m_stmts);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Halt&, int depth = 0) const
	{
		PrintWithIndentation(depth, "Halt");
	}

	void operator()(const Binary& binary, int depth = 0) const
	{
		PrintWithIndentation(depth, "Binary {");
		PrintWithIndentation(depth + 1, "Operator: " + binary.m_op.m_lexeme);
		PrintWithIndentation(depth + 1, "Left: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *binary.m_left);
		PrintWithIndentation(depth + 1, "Right: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *binary.m_right);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Pipe& pipe, int depth = 0) const
	{
		PrintWithIndentation(depth, "Pipe {");
		PrintWithIndentation(depth + 1, "Left: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *pipe.m_left);
		PrintWithIndentation(depth + 1, "Right: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *pipe.m_right);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Logical& logical, int depth = 0) const
	{
		PrintWithIndentation(depth, "Logical {");
		PrintWithIndentation(depth + 1, "Operator: " + logical.m_op.m_lexeme);
		PrintWithIndentation(depth + 1, "Left: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *logical.m_left);
		PrintWithIndentation(depth + 1, "Right: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *logical.m_right);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Group& group, int depth = 0) const
	{
		PrintWithIndentation(depth, "Group {");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 1); }, *group.m_expr_in);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Unary& unary, int depth = 0) const
	{
		PrintWithIndentation(depth, "Unary {");
		PrintWithIndentation(depth + 1, "Operator: " + unary.m_op.m_lexeme);
		PrintWithIndentation(depth + 1, "Right: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *unary.m_right);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Call& call, int depth = 0) const
	{
		PrintWithIndentation(depth, "Call {");
		PrintWithIndentation(depth + 1, "Callee: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *call.m_callee);
		PrintWithIndentation(depth + 1, "Args: ");
		for (const std::unique_ptr<Expression>& arg : call.m_arguments)
		{
			std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *arg);
		}
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Get& get, int depth = 0) const
	{
		PrintWithIndentation(depth, "Get {");
		PrintWithIndentation(depth + 1, "Object: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *get.m_object);
		PrintWithIndentation(depth + 1, "Name: " + get.m_name.m_lexeme);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Set& set, int depth = 0) const
	{
		PrintWithIndentation(depth, "Set {");
		PrintWithIndentation(depth + 1, "Object: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *set.m_object);
		PrintWithIndentation(depth + 1, "Name: " + set.m_name.m_lexeme);
		PrintWithIndentation(depth + 1, "Value: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *set.m_value);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const This&, int depth = 0) const
	{
		PrintWithIndentation(depth, "This");
	}

	void operator()(const Super& super, int depth = 0) const
	{
		PrintWithIndentation(depth, "Super {");
		PrintWithIndentation(depth + 1, "Method: " + super.m_method.m_lexeme);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Variable& variable, int depth = 0) const
	{
		PrintWithIndentation(depth, "Variable {");
		PrintWithIndentation(depth + 1, "Name: " + variable.m_name.m_lexeme);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Assign& assign, int depth = 0) const
	{
		PrintWithIndentation(depth, "Assign {");
		PrintWithIndentation(depth + 1, "Name: " + assign.m_name.m_lexeme);
		PrintWithIndentation(depth + 1, "Value: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *assign.m_value);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Literal& literal, int depth = 0) const
	{
		PrintWithIndentation(depth, "Literal {");
		PrintWithIndentation(depth + 1, "Value: " + literal.m_value.m_lexeme);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Lambda& lambda, int depth = 0) const
	{
		PrintWithIndentation(depth, "Lambda {");
		PrintWithIndentation(depth + 1, "Params: ");
		for (const Token& param : lambda.m_params)
		{
			PrintWithIndentation(depth + 2, param.m_lexeme);
		}
		PrintWithIndentation(depth + 1, "Body: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *lambda.m_body);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Array& array, int depth = 0) const
	{
		PrintWithIndentation(depth, "Array {");
		PrintWithIndentation(depth + 1, "Elements: ");
		for (const std::unique_ptr<Expression>& element : array.m_elems)
		{
			std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *element);
		}
		PrintWithIndentation(depth, "}");
	}

	void operator()(const ArrayGet& array_get, int depth = 0) const
	{
		PrintWithIndentation(depth, "ArrayGet {");
		PrintWithIndentation(depth + 1, "Array: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *array_get.m_arr_var);
		PrintWithIndentation(depth + 1, "Index: ");
		for (const std::unique_ptr<Expression>& index : array_get.m_indices)
		{
			std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *index);
		}
		PrintWithIndentation(depth, "}");
	}

	void operator()(const ArraySet& array_set, int depth = 0) const
	{
		PrintWithIndentation(depth, "ArraySet {");
		PrintWithIndentation(depth + 1, "Array: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *array_set.m_arr_var);
		PrintWithIndentation(depth + 1, "Index: ");
		for (const std::unique_ptr<Expression>& index : array_set.m_indices)
		{
			std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *index);
		}
		PrintWithIndentation(depth + 1, "Value: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *array_set.m_value);
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Comma& comma, int depth = 0) const
	{
		PrintWithIndentation(depth, "Comma {");
		for (const std::unique_ptr<Expression>& expr : comma.m_exprs)
		{
			std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 1); }, *expr);
		}
		PrintWithIndentation(depth, "}");
	}

	void operator()(const Ternary& ternary, int depth = 0) const
	{
		PrintWithIndentation(depth, "Ternary {");
		PrintWithIndentation(depth + 1, "Condition: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *ternary.m_condition);
		PrintWithIndentation(depth + 1, "Then: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *ternary.m_true_branch);
		PrintWithIndentation(depth + 1, "Else: ");
		std::visit([depth](auto&& arg) { AbstractSyntaxTreePrinter()(arg, depth + 2); }, *ternary.m_else_branch);
		PrintWithIndentation(depth, "}");
	}
};
