#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"

#include <unordered_map>
#include <array>

class TypeChecker
{
public:

private:
	using TypeEnvironment = std::unordered_map<std::string, std::shared_ptr<MidoriType>>;
	using TypeEnvironmentStack = std::vector<TypeEnvironment>;

	std::string m_errors;
	const std::array<Token::Name, 5> m_binary_arithmetic_operators = { Token::Name::SINGLE_PLUS, Token::Name::MINUS, Token::Name::STAR, Token::Name::SLASH, Token::Name::PERCENT };
	const std::array<Token::Name, 1> m_binary_concatenation_operators = { Token::Name::DOUBLE_PLUS };
	const std::array<Token::Name, 4> m_binary_partial_order_comparison_operators = { Token::Name::LEFT_ANGLE, Token::Name::LESS_EQUAL, Token::Name::RIGHT_ANGLE, Token::Name::GREATER_EQUAL };
	const std::array<Token::Name, 2> m_binary_equality_operators = { Token::Name::DOUBLE_EQUAL, Token::Name::BANG_EQUAL };
	const std::array<Token::Name, 2> m_binary_logical_operators = { Token::Name::DOUBLE_AMPERSAND, Token::Name::DOUBLE_BAR };
	const std::array<Token::Name, 5> m_binary_bitwise_operators = { Token::Name::CARET, Token::Name::SINGLE_AMPERSAND, Token::Name::SINGLE_BAR, Token::Name::RIGHT_SHIFT, Token::Name::LEFT_SHIFT };
	const std::array<MidoriType, 5> m_atomic_types = { FractionType{}, IntegerType{}, TextType{}, BoolType{}, UnitType{} };
	TypeEnvironmentStack m_name_type_table;
	const MidoriType* m_curr_closure_return_type = nullptr;

public:

	MidoriResult::TypeCheckerResult TypeCheck(MidoriProgramTree& program_tree);

private:

	void AddError(std::string&& error);

	void BeginScope();

	void EndScope();

	void operator()(Block& block);

	void operator()(Simple& simple);

	void operator()(Define& def);

	void operator()(If& if_stmt);

	void operator()(While& while_stmt);

	void operator()(For& for_stmt);

	void operator()(Break& break_stmt);

	void operator()(Continue& continue_stmt);

	void operator()(Return& return_stmt);

	void operator()(Foreign& foreign);

	void operator()(Struct& struct_stmt);

	MidoriResult::TypeResult operator()(As& as);

	MidoriResult::TypeResult operator()(Binary& binary);

	MidoriResult::TypeResult operator()(Group& group);

	MidoriResult::TypeResult operator()(Unary& unary);

	MidoriResult::TypeResult operator()(Call& call);

	MidoriResult::TypeResult operator()(Get& get);

	MidoriResult::TypeResult operator()(Set& set);

	MidoriResult::TypeResult operator()(Variable& variable);

	MidoriResult::TypeResult operator()(Bind& bind);

	MidoriResult::TypeResult operator()(TextLiteral& text);

	MidoriResult::TypeResult operator()(BoolLiteral& bool_expr);

	MidoriResult::TypeResult operator()(FractionLiteral& fraction);

	MidoriResult::TypeResult operator()(IntegerLiteral& integer);

	MidoriResult::TypeResult operator()(UnitLiteral& unit);

	MidoriResult::TypeResult operator()(Closure& closure);

	MidoriResult::TypeResult operator()(Construct& construct);

	MidoriResult::TypeResult operator()(Array& array);

	MidoriResult::TypeResult operator()(ArrayGet& array_get);

	MidoriResult::TypeResult operator()(ArraySet& array_set);

	MidoriResult::TypeResult operator()(Ternary& ternary);
};
