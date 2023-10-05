#pragma once

#include <optional>
#include <memory>
#include <vector>
#include <variant>

#include "Compiler/Token/Token.h"

struct Binary;
struct Pipe;
struct Logical;
struct Group;
struct String;
struct Bool;
struct Number;
struct Nil;
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

using Expression = std::variant < Binary, Pipe, Logical, Group, String, Bool, Number, Nil, Unary, Assign, Variable, Call, Function, Ternary, Get, Set, Array, ArrayGet, ArraySet>;

//////////////////////////////////////////////////////////////////////////////////////////////////////

struct Block;
struct Simple;
struct Let;
struct If;
struct While;
struct For;
struct Break;
struct Continue;
struct Return;
struct Import;
struct Namespace;
struct Halt;

using Statement = std::variant<Block, Simple, Let, If, While, For, Break, Continue, Return, Import, Namespace, Halt>;
using ProgramTree = std::vector<std::unique_ptr<Statement>>;

//////////////////////////////////////////////////////////////////////////////////////////////////////


struct Binary
{
	Token m_op;
	std::unique_ptr<Expression> m_left;
	std::unique_ptr<Expression> m_right;
};

struct Pipe
{
	Token m_op;
	std::unique_ptr<Expression> m_left;
	std::unique_ptr<Expression> m_right;
};

struct Logical
{
	Token m_op;
	std::unique_ptr<Expression> m_left;
	std::unique_ptr<Expression> m_right;
};

struct Group
{
	std::unique_ptr<Expression> m_expr_in;
};

struct String
{
	Token m_token;
};

struct Bool
{
	Token m_token;
};

struct Number
{
	Token m_token;
};

struct Nil
{
	Token m_token;
};

struct Unary
{
	Token m_op;
	std::unique_ptr<Expression> m_right;
};

struct Assign
{
	Token m_name;
	std::unique_ptr<Expression> m_value;
	std::optional<int> m_stack_offset;
};

struct Variable
{
	Token m_name;
	std::optional<int> m_stack_offset;
};

struct Call
{
	Token m_paren;
	std::unique_ptr<Expression> m_callee;
	std::vector<std::unique_ptr<Expression>> m_arguments;
};

struct Function
{
	Token m_function_keyword;
	std::vector<Token> m_params;
	std::unique_ptr<Statement> m_body;
};

struct Ternary
{
	Token m_question;
	Token m_colon;
	std::unique_ptr<Expression> m_condition;
	std::unique_ptr<Expression> m_true_branch;
	std::unique_ptr<Expression> m_else_branch;
};

struct Get
{
	Token m_name;
	std::unique_ptr<Expression> m_object;
};

struct Set
{
	Token m_name;
	std::unique_ptr<Expression> m_object;
	std::unique_ptr<Expression> m_value;
};

struct Array
{
	Token m_op;
	std::vector<std::unique_ptr<Expression>> m_elems;
	std::optional<std::unique_ptr<Expression>> m_allocated_size;
};

struct ArrayGet
{
	Token m_op;
	std::unique_ptr<Expression> m_arr_var;
	std::vector<std::unique_ptr<Expression>> m_indices;
};

struct ArraySet
{
	Token m_op;
	std::unique_ptr<Expression> m_arr_var;
	std::vector<std::unique_ptr<Expression>> m_indices;
	std::unique_ptr<Expression> m_value;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////


struct Block
{
	Token m_right_brace;
	std::vector<std::unique_ptr<Statement>> m_stmts;
	int m_local_count;
};

struct Simple
{
	std::unique_ptr<Expression> m_expr;
};

struct Let
{
	Token m_name;
	std::optional<std::unique_ptr<Expression>> m_value;
	std::optional<int> m_local_index;
};

struct If
{
	Token m_if_keyword;
	std::optional<Token> m_else_keyword;
	std::unique_ptr<Expression> m_condition;
	std::unique_ptr<Statement> m_true_branch;
	std::optional<std::unique_ptr<Statement>> m_else_branch;
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
	std::unique_ptr<Statement> m_condition_intializer;
	std::optional<std::unique_ptr<Expression>> m_condition;
	std::optional<std::unique_ptr<Statement>> m_condition_incrementer;
	std::unique_ptr<Statement> m_body;
	int m_control_block_local_count;
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
	std::optional<std::unique_ptr<Expression>> m_value;
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

struct Halt
{
	Token m_keyword;
	std::unique_ptr<Expression> m_message;
};
