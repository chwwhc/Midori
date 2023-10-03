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
struct Lambda;
struct Ternary;
struct Get;
struct Set;
struct Array;
struct ArrayGet;
struct ArraySet;

using Expression = std::variant < Binary, Pipe, Logical, Group, String, Bool, Number, Nil, Unary, Assign, Variable, Call, Lambda, Ternary, Get, Set, Array, ArrayGet, ArraySet> ;

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

struct Lambda
{
	Token m_name;
	std::vector<Token> m_params;
	std::unique_ptr<Expression> m_body;
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