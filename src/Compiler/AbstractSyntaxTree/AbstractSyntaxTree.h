#pragma once

#include <optional>
#include <memory>
#include <vector>
#include <variant>

#include "Compiler/Token/Token.h"

struct Binary;
struct Logical;
struct Group;
struct String;
struct Bool;
struct Number;
struct Unit;
struct Unary;
struct Assign;
struct Variable;
struct Call;
struct Function;
struct Ternary;
struct Get;
struct Set;
struct Array;
struct ArrayGet;
struct ArraySet;

using Expression = std::variant < Binary, Logical, Group, String, Bool, Number, Unit, Unary, Assign, Variable, Call, Function, Ternary, Get, Set, Array, ArrayGet, ArraySet>;

//////////////////////////////////////////////////////////////////////////////////////////////////////

struct Block;
struct Simple;
struct Define;
struct If;
struct While;
struct For;
struct Break;
struct Continue;
struct Return;
struct Import;
struct Namespace;

using Statement = std::variant<Block, Simple, Define, If, While, For, Break, Continue, Return, Import, Namespace>;
using ProgramTree = std::vector<std::unique_ptr<Statement>>;

//////////////////////////////////////////////////////////////////////////////////////////////////////

namespace VariableSemantic
{
	struct Local
	{
		int m_index = 0;
	};
	struct Cell
	{
		int m_index = 0;
	};
	struct Global {};
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

struct NumberType {};
struct StringType {};
struct BoolType {};
struct UnitType {};
struct AnyType {};
struct ArrayType;
struct FunctionType;

using Type = std::variant<NumberType, StringType, BoolType, UnitType, AnyType, ArrayType, FunctionType>;

struct ArrayType
{
	std::unique_ptr<Type> m_element_type;
};

struct FunctionType
{
	std::vector<std::unique_ptr<Type>> m_param_types;
	std::unique_ptr<Type> m_return_type;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

struct Binary
{
	Token m_op;
	std::unique_ptr<Expression> m_left;
	std::unique_ptr<Expression> m_right;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Logical
{
	Token m_op;
	std::unique_ptr<Expression> m_left;
	std::unique_ptr<Expression> m_right;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Group
{
	std::unique_ptr<Expression> m_expr_in;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct String
{
	Token m_token;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(StringType());
};

struct Bool
{
	Token m_token;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(BoolType());
};

struct Number
{
	Token m_token;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(NumberType());
};

struct Unit
{
	Token m_token;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(UnitType());
};

struct Unary
{
	Token m_op;
	std::unique_ptr<Expression> m_right;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Assign
{
	Token m_name;
	std::unique_ptr<Expression> m_value;
	std::variant<VariableSemantic::Local, VariableSemantic::Global, VariableSemantic::Cell> m_semantic;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Variable
{
	Token m_name;
	std::variant<VariableSemantic::Local, VariableSemantic::Global, VariableSemantic::Cell> m_semantic;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Call
{
	Token m_paren;
	std::unique_ptr<Expression> m_callee;
	std::vector<std::unique_ptr<Expression>> m_arguments;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Function
{
	Token m_function_keyword;
	std::vector<Token> m_params;
	std::unique_ptr<Statement> m_body;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Ternary
{
	Token m_question;
	Token m_colon;
	std::unique_ptr<Expression> m_condition;
	std::unique_ptr<Expression> m_true_branch;
	std::unique_ptr<Expression> m_else_branch;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Get
{
	Token m_name;
	std::unique_ptr<Expression> m_object;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Set
{
	Token m_name;
	std::unique_ptr<Expression> m_object;
	std::unique_ptr<Expression> m_value;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct Array
{
	Token m_op;
	std::vector<std::unique_ptr<Expression>> m_elems;
	std::optional<std::unique_ptr<Expression>> m_allocated_size;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct ArrayGet
{
	Token m_op;
	std::unique_ptr<Expression> m_arr_var;
	std::vector<std::unique_ptr<Expression>> m_indices;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

struct ArraySet
{
	Token m_op;
	std::unique_ptr<Expression> m_arr_var;
	std::vector<std::unique_ptr<Expression>> m_indices;
	std::unique_ptr<Expression> m_value;
	std::unique_ptr<Type> m_type = std::make_unique<Type>(AnyType());
};

//////////////////////////////////////////////////////////////////////////////////////////////////////


struct Block
{
	Token m_right_brace;
	std::vector<std::unique_ptr<Statement>> m_stmts;
	int m_local_count = 0;
};

struct Simple
{
	Token m_semicolon;
	std::unique_ptr<Expression> m_expr;
};

struct Define
{
	Token m_name;
	std::unique_ptr<Expression> m_value;
	int m_local_index;
	bool m_is_fixed;
};

struct If
{
	Token m_if_keyword;
	std::optional<Token> m_else_keyword;
	std::optional<std::unique_ptr<Statement>> m_else_branch;
	std::unique_ptr<Expression> m_condition;
	std::unique_ptr<Statement> m_true_branch;
};

struct While
{
	Token m_while_keyword;
	std::unique_ptr<Expression> m_condition;
	std::unique_ptr<Statement> m_body;
};

struct For
{
	Token m_for_keyword;
	std::optional<std::unique_ptr<Expression>> m_condition;
	std::optional<std::unique_ptr<Statement>> m_condition_incrementer;
	std::unique_ptr<Statement> m_condition_intializer;
	std::unique_ptr<Statement> m_body;
	int m_control_block_local_count = 0;
};

struct Break
{
	Token m_keyword;
};

struct Continue
{
	Token m_keyword;
};

struct Return
{
	Token m_keyword;
	std::unique_ptr<Expression> m_value;
};

struct Import
{
	Token m_keyword;
	std::unique_ptr<Expression> m_path;
};

struct Namespace
{
	Token m_name;
	std::unique_ptr<Statement> m_stmts;
};