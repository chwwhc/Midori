#pragma once

#include <optional>
#include <memory>

#include "Compiler/Token/Token.h"
#include "Common/Value/Type.h"

struct As;
struct Binary;
struct Group;
struct Literal;
struct TextLiteral;
struct BoolLiteral;
struct FractionLiteral;
struct IntegerLiteral;
struct UnitLiteral;
struct Unary;
struct Bind;
struct Variable;
struct Call;
struct Closure;
struct Construct;
struct Ternary;
struct Get;
struct Set;
struct Array;
struct ArrayGet;
struct ArraySet;

using MidoriExpression = std::variant < As, Binary, Group, TextLiteral, BoolLiteral, FractionLiteral, IntegerLiteral, UnitLiteral, Unary, Bind, Variable, Call, Closure, Construct, Ternary, Get, Set, Array, ArrayGet, ArraySet>;

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
struct Foreign;
struct Struct;

using MidoriStatement = std::variant<Block, Simple, Define, If, While, For, Break, Continue, Return, Foreign, Struct>;
using MidoriProgramTree = std::vector<std::unique_ptr<MidoriStatement>>;

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

struct As
{
	Token m_as_keyword;
	std::unique_ptr<MidoriExpression> m_expr;
	std::shared_ptr<MidoriType> m_target_type;
};

struct Binary
{
	Token m_op;
	std::unique_ptr<MidoriExpression> m_left;
	std::unique_ptr<MidoriExpression> m_right;
	std::shared_ptr<MidoriType> m_type;
};

struct Group
{
	std::unique_ptr<MidoriExpression> m_expr_in;
};

struct TextLiteral
{
	Token m_token;
};

struct BoolLiteral
{
	Token m_token;
};

struct FractionLiteral
{
	Token m_token;
};

struct IntegerLiteral
{
	Token m_token;
};

struct UnitLiteral
{
	Token m_token;
};

struct Unary
{
	Token m_op;
	std::unique_ptr<MidoriExpression> m_right;
	std::shared_ptr<MidoriType> m_type;
};

struct Bind
{
	Token m_name;
	std::unique_ptr<MidoriExpression> m_value;
	std::variant<VariableSemantic::Local, VariableSemantic::Global, VariableSemantic::Cell> m_semantic;
};

struct Variable
{
	Token m_name;
	std::variant<VariableSemantic::Local, VariableSemantic::Global, VariableSemantic::Cell> m_semantic;
};

struct Call
{
	Token m_paren;
	std::unique_ptr<MidoriExpression> m_callee;
	std::vector<std::unique_ptr<MidoriExpression>> m_arguments;
	bool m_is_foreign = false;
};

struct Closure
{
	Token m_closure_keyword;
	std::vector<Token> m_params;
	std::vector<std::shared_ptr<MidoriType>> m_param_types;
	std::unique_ptr<MidoriStatement> m_body;
	std::shared_ptr<MidoriType> m_return_type;
	int m_captured_count = 0;
};

struct Construct
{
	Token m_type_name;
	std::vector<std::unique_ptr<MidoriExpression>> m_params;
	std::shared_ptr<MidoriType> m_return_type;
};

struct Ternary
{
	Token m_question;
	Token m_colon;
	std::unique_ptr<MidoriExpression> m_condition;
	std::unique_ptr<MidoriExpression> m_true_branch;
	std::unique_ptr<MidoriExpression> m_else_branch;
};

struct Get
{
	Token m_member_name;
	std::unique_ptr<MidoriExpression> m_struct;
	int m_index = -1;
};

struct Set
{
	Token m_member_name;
	std::unique_ptr<MidoriExpression> m_struct;
	std::unique_ptr<MidoriExpression> m_value;
	int m_index = -1;
};

struct Array
{
	Token m_op;
	std::vector<std::unique_ptr<MidoriExpression>> m_elems;
};

struct ArrayGet
{
	Token m_op;
	std::unique_ptr<MidoriExpression> m_arr_var;
	std::vector<std::unique_ptr<MidoriExpression>> m_indices;
};

struct ArraySet
{
	Token m_op;
	std::unique_ptr<MidoriExpression> m_arr_var;
	std::vector<std::unique_ptr<MidoriExpression>> m_indices;
	std::unique_ptr<MidoriExpression> m_value;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////


struct Block
{
	Token m_right_brace;
	std::vector<std::unique_ptr<MidoriStatement>> m_stmts;
	int m_local_count = 0;
};

struct Simple
{
	Token m_semicolon;
	std::unique_ptr<MidoriExpression> m_expr;
};

struct Define
{
	Token m_name;
	std::unique_ptr<MidoriExpression> m_value;
	std::optional<std::shared_ptr<MidoriType>> m_annotated_type;
	std::optional<int> m_local_index;
};

struct If
{
	Token m_if_keyword;
	std::optional<Token> m_else_keyword;
	std::optional<std::unique_ptr<MidoriStatement>> m_else_branch;
	std::unique_ptr<MidoriExpression> m_condition;
	std::unique_ptr<MidoriStatement> m_true_branch;
};

struct While
{
	Token m_while_keyword;
	std::unique_ptr<MidoriExpression> m_condition;
	std::unique_ptr<MidoriStatement> m_body;
};

struct For
{
	Token m_for_keyword;
	std::optional<std::unique_ptr<MidoriExpression>> m_condition;
	std::optional<std::unique_ptr<MidoriStatement>> m_condition_incrementer;
	std::optional<std::unique_ptr<MidoriStatement>> m_condition_intializer;
	std::unique_ptr<MidoriStatement> m_body;
	int m_control_block_local_count = 0;
};

struct Break
{
	Token m_keyword;
	int m_number_to_pop = 0;
};

struct Continue
{
	Token m_keyword;
	int m_number_to_pop = 0;
};

struct Return
{
	Token m_keyword;
	std::unique_ptr<MidoriExpression> m_value;
};

struct Foreign
{
	Token m_function_name;
	std::shared_ptr<MidoriType> m_type;
	std::optional<int> m_local_index;
};

struct Struct
{
	Token m_name;
	std::shared_ptr<MidoriType> m_self_type;
};