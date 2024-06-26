#pragma once

#include <optional>
#include <memory>

#include "Compiler/Token/Token.h"
#include "Common/Type/Type.h"

struct As;
struct Binary;
struct Group;
struct Literal;
struct TextLiteral;
struct BoolLiteral;
struct FractionLiteral;
struct IntegerLiteral;
struct UnitLiteral;
struct UnaryPrefix;
struct UnarySuffix;
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

using MidoriExpression = std::variant <As, Binary, Group, TextLiteral, BoolLiteral, FractionLiteral, IntegerLiteral, UnitLiteral, UnaryPrefix, UnarySuffix,Bind, Variable, Call, Closure, Construct, Ternary, Get, Set, Array, ArrayGet, ArraySet>;

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
struct Union;
struct Switch;
struct Namespace;

using MidoriStatement = std::variant<Block, Simple, Define, If, While, For, Break, Continue, Return, Foreign, Struct, Union, Switch, Namespace>;
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

	using Tag = std::variant<Local, Cell, Global>;
}

enum class ConditionOperandType
{
	INTEGER,
	FRACTION,
	OTHER
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

struct As
{
	Token m_as_keyword;
	std::unique_ptr<MidoriExpression> m_expr;
	const MidoriType* m_target_type;
};

struct Binary
{
	Token m_op;
	std::unique_ptr<MidoriExpression> m_left;
	std::unique_ptr<MidoriExpression> m_right;
	const MidoriType* m_type;
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

struct UnaryPrefix
{
	Token m_op;
	std::unique_ptr<MidoriExpression> m_expr;
	const MidoriType* m_type;
};

struct UnarySuffix
{
	Token m_op;
	std::unique_ptr<MidoriExpression> m_expr;
	const MidoriType* m_type;
};

struct Bind
{
	Token m_name;
	std::unique_ptr<MidoriExpression> m_value;
	VariableSemantic::Tag m_semantic_tag;
};

struct Variable
{
	Token m_name;
	VariableSemantic::Tag m_semantic_tag;
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
	std::vector<const MidoriType*> m_param_types;
	std::unique_ptr<MidoriStatement> m_body;
	const MidoriType* m_return_type;
	int m_captured_count = 0;
};

struct Construct
{
	struct Union
	{
		int m_index;
	};
	struct Struct {};
	using ConstructContext = std::variant<Union, Struct>;

	Token m_data_name;
	std::vector<std::unique_ptr<MidoriExpression>> m_params;
	const MidoriType* m_return_type;
	ConstructContext m_construct_ctx;
};

struct Ternary
{
	Token m_question;
	Token m_colon;
	std::unique_ptr<MidoriExpression> m_condition;
	std::unique_ptr<MidoriExpression> m_true_branch;
	std::unique_ptr<MidoriExpression> m_else_branch;
	ConditionOperandType m_condition_operand_type;
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
	std::optional<const MidoriType*> m_annotated_type;
	std::optional<int> m_local_index;
};

struct If
{
	Token m_if_keyword;
	std::optional<Token> m_else_keyword;
	std::optional<std::unique_ptr<MidoriStatement>> m_else_branch;
	std::unique_ptr<MidoriExpression> m_condition;
	std::unique_ptr<MidoriStatement> m_true_branch;
	ConditionOperandType m_condition_operand_type;
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
	std::string m_foreign_name;
	const MidoriType* m_type;
	std::optional<int> m_local_index;
};

struct Struct
{
	Token m_name;
	const MidoriType* m_self_type;
};

struct Union
{
	Token m_name;
	const MidoriType* m_self_type;
};

struct Switch
{
	struct MemberCase
	{
		Token m_keyword;
		std::vector<std::string> m_binding_names;
		std::string m_member_name;
		std::unique_ptr<MidoriStatement> m_stmt;
		int m_tag;
	};

	struct DefaultCase
	{
		Token m_keyword;
		std::unique_ptr<MidoriStatement> m_stmt;
	};

	using Case = std::variant<MemberCase, DefaultCase>;

	Token m_switch_keyword;
	std::unique_ptr<MidoriExpression> m_arg_expr;
	std::vector<Case> m_cases;

	static bool IsMemberCase(const Case& c);

	static bool IsDefaultCase(const Case& c);

	static MemberCase& GetMemberCase(const Case& c);

	static DefaultCase& GetDefaultCase(const Case& c);

	static const Token& GetKeyword(const Case& c);

	static const std::unique_ptr<MidoriStatement>& GetCaseStatement(const Case& c);
};

struct Namespace
{
	Token m_name;
	std::vector<std::unique_ptr<MidoriStatement>> m_stmts;
};