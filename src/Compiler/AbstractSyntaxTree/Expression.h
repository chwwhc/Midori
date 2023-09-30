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
struct This;
struct Super;
struct Array;
struct ArrayGet;
struct ArraySet;
struct Comma;

using Expression = std::variant < Binary, Pipe, Logical, Group, String, Bool, Number, Nil, Unary, Assign, Variable, Call, Lambda, Ternary, Get, Set, This, Super, Array, ArrayGet, ArraySet, Comma > ;

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
	std::string m_value;
};

struct Bool
{
	bool m_value;
};

struct Number
{
	double m_value;
};

struct Nil
{

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
};

struct Variable
{
	Token m_name;
	bool m_is_fixed;
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

struct This
{
	Token m_keyword;
};

struct Super
{
	Token m_keyword;
	Token m_method;
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

struct Comma
{
	std::vector<std::unique_ptr<Expression>> m_exprs;
};