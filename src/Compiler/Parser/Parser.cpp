#include "Parser.h"
#include "Compiler/Lexer/Lexer.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <queue>

#define VERIFY_RESULT(result) \
    if (!(result).has_value()) \
    { \
        return std::unexpected<std::string>(std::move((result).error())); \
    } \
    else \
    (void)0

Parser::Parser(TokenStream&& tokens, const std::string& file_name) : m_tokens(std::move(tokens))
{
	std::string absolute_file_path = std::filesystem::absolute(file_name).string();
	s_dependency_graph[absolute_file_path] = {};
	m_file_name = std::move(absolute_file_path);
}

bool Parser::IsGlobalName(const std::vector<Scope>::const_reverse_iterator& found_scope_it) const
{
	return found_scope_it == std::prev(m_scopes.crend());
}

bool Parser::IsLocalName(const Scope::VariableTable::const_iterator& found_tbl_it) const
{
	return m_closure_depth == 0 || found_tbl_it->second.m_closure_depth == m_closure_depth;
}

bool Parser::IsAtGlobalScope() const
{
	return m_scopes.size() == 1u;
}

std::string Parser::GenerateParserError(std::string&& message, const Token& token)
{
	Synchronize();
	return MidoriError::GenerateParserError(std::move(message), token);
}

bool Parser::IsAtEnd()
{
	return Peek(0).m_token_name == Token::Name::END_OF_FILE;
}

bool Parser::Check(Token::Name type, int offset)
{
	return !IsAtEnd() && Peek(offset).m_token_name == type;
}

Token& Parser::Peek(int offset)
{
	return m_current_token_index + offset < m_tokens.Size() ? m_tokens[m_current_token_index + offset] : m_tokens[m_tokens.Size() - 1];
}

Token& Parser::Previous()
{
	return m_tokens[m_current_token_index - 1];
}

Token& Parser::Advance()
{
	if (!IsAtEnd())
	{
		m_current_token_index += 1;
	}
	return Previous();
}

MidoriResult::TokenResult Parser::Consume(Token::Name type, std::string_view message)
{
	if (Check(type, 0))
	{
		return Advance();
	}

	return std::unexpected<std::string>(MidoriError::GenerateParserError(message, Previous()));
}

void Parser::BeginScope()
{
	m_scopes.emplace_back(Scope());
}

int Parser::EndScope()
{
	const Scope& scope = m_scopes.back();
	int block_local_count = static_cast<int>(scope.m_variables.size() - scope.m_structs.size()); // struct count is subtracted because they do not take up space in the stack
	m_total_locals_in_curr_scope -= block_local_count;
	m_total_variables -= block_local_count;
	m_scopes.pop_back();
	return block_local_count;
}

MidoriResult::TokenResult Parser::DefineName(const Token& name, bool is_fixed)
{
	if (m_scopes.back().m_variables.contains(name.m_lexeme))
	{
		return std::unexpected<std::string>(GenerateParserError("Variable with this name already exists", name));
	}

	for (int i = static_cast<int>(m_scopes.size()) - 2; i >= 0; --i)
	{
		if (m_scopes[i].m_structs.contains(name.m_lexeme))
		{
			// TODO: Warning
			// Overshadowing a struct
		}
		if (m_scopes[i].m_variables.contains(name.m_lexeme))
		{
			// TODO: Warning
			// Overshadowing a variable
		}
	}

	m_scopes.back().m_variables.emplace(name.m_lexeme, VariableContext(is_fixed));

	return name;
}

std::optional<int> Parser::GetLocalVariableIndex(const std::string& name, bool is_fixed)
{
	std::optional<int> local_index = std::nullopt;

	if (!IsAtGlobalScope())
	{
		m_scopes.back().m_variables[name] = VariableContext(m_total_locals_in_curr_scope++, m_total_variables++, m_closure_depth, is_fixed);
		local_index.emplace(m_scopes.back().m_variables[name].m_relative_index.value());
	}

	return local_index;
}

MidoriResult::ExpressionResult Parser::ParseFactor()
{
	return ParseBinary(&Parser::ParseUnary, Token::Name::STAR, Token::Name::SLASH, Token::Name::PERCENT);
}

MidoriResult::ExpressionResult Parser::ParseShift()
{
	return ParseBinary(&Parser::ParseTerm, Token::Name::LEFT_SHIFT, Token::Name::RIGHT_SHIFT);
}

MidoriResult::ExpressionResult Parser::ParseTerm()
{
	return ParseBinary(&Parser::ParseFactor, Token::Name::SINGLE_PLUS, Token::Name::DOUBLE_PLUS, Token::Name::MINUS);
}

MidoriResult::ExpressionResult Parser::ParseComparison()
{
	return ParseBinary(&Parser::ParseShift, Token::Name::LEFT_ANGLE, Token::Name::LESS_EQUAL, Token::Name::RIGHT_ANGLE, Token::Name::GREATER_EQUAL);
}

MidoriResult::ExpressionResult Parser::ParseEquality()
{
	return ParseBinary(&Parser::ParseComparison, Token::Name::BANG_EQUAL, Token::Name::DOUBLE_EQUAL);
}

MidoriResult::ExpressionResult Parser::ParseBitwiseAnd()
{
	return ParseBinary(&Parser::ParseEquality, Token::Name::SINGLE_AMPERSAND);
}

MidoriResult::ExpressionResult Parser::ParseBitwiseXor()
{
	return ParseBinary(&Parser::ParseBitwiseAnd, Token::Name::CARET);
}

MidoriResult::ExpressionResult Parser::ParseBitwiseOr()
{
	return ParseBinary(&Parser::ParseBitwiseXor, Token::Name::SINGLE_BAR);
}

MidoriResult::ExpressionResult Parser::ParseBind()
{
	MidoriResult::ExpressionResult expr = ParseTernary();
	VERIFY_RESULT(expr);

	if (Match(Token::Name::SINGLE_EQUAL))
	{
		Token& equal = Previous();
		MidoriResult::ExpressionResult value = ParseBind();
		VERIFY_RESULT(value);

		if (std::holds_alternative<Variable>(*expr.value()))
		{
			Variable& variable_expr = std::get<Variable>(*expr.value());

			std::vector<Scope>::const_reverse_iterator found_scope_it = std::find_if(m_scopes.crbegin(), m_scopes.crend(), [&variable_expr](const Scope& scope)
				{
					return scope.m_variables.contains(variable_expr.m_name.m_lexeme);
				});

			if (found_scope_it != m_scopes.crend())
			{
				Scope::VariableTable::const_iterator find_result = found_scope_it->m_variables.find(variable_expr.m_name.m_lexeme);

				if (find_result->second.m_is_fixed)
				{
					return std::unexpected<std::string>(GenerateParserError("Cannot break a fixed name binding.", variable_expr.m_name));
				}

				if (IsGlobalName(found_scope_it))
				{
					return std::make_unique<MidoriExpression>(Bind{ std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Global() });
				}
				else if (IsLocalName(find_result))
				{
					return std::make_unique<MidoriExpression>(Bind{ std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Local(find_result->second.m_relative_index.value()) });
				}
				// cell
				else
				{
					return std::make_unique<MidoriExpression>(Bind{ std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Cell(find_result->second.m_absolute_index.value()) });
				}
			}

			return std::unexpected<std::string>(GenerateParserError("Undefined Variable.", variable_expr.m_name));
		}
		else if (std::holds_alternative<Get>(*expr.value()))
		{
			Get& get_expr = std::get<Get>(*expr.value());
			return std::make_unique<MidoriExpression>(Set{ std::move(get_expr.m_member_name), std::move(get_expr.m_struct), std::move(value.value()) });
		}
		else if (std::holds_alternative<ArrayGet>(*expr.value()))
		{
			ArrayGet& access_expr = std::get<ArrayGet>(*expr.value());
			return std::make_unique<MidoriExpression>(ArraySet{ std::move(access_expr.m_op), std::move(access_expr.m_arr_var), std::move(access_expr.m_indices), std::move(value.value()) });
		}

		return std::unexpected<std::string>(GenerateParserError("Invalid binding target.", equal));
	}

	return expr;
}

MidoriResult::ExpressionResult Parser::ParseUnary()
{
	if (Match(Token::Name::BANG, Token::Name::MINUS, Token::Name::TILDE))
	{
		Token& op = Previous();
		MidoriResult::ExpressionResult right = ParseUnary();
		VERIFY_RESULT(right);

		return std::make_unique<MidoriExpression>(Unary{ std::move(op), std::move(right.value()) });
	}

	return ParseConstruct();
}

MidoriResult::ExpressionResult Parser::ParseTernary()
{
	MidoriResult::ExpressionResult condition = ParseLogicalOr();
	VERIFY_RESULT(condition);

	if (Match(Token::Name::QUESTION))
	{
		Token& question = Previous();
		MidoriResult::ExpressionResult true_branch = ParseTernary();
		VERIFY_RESULT(true_branch);

		if (Match(Token::Name::SINGLE_COLON))
		{
			Token& colon = Previous();
			MidoriResult::ExpressionResult else_branch = ParseTernary();
			VERIFY_RESULT(else_branch);

			return std::make_unique<MidoriExpression>(Ternary{ std::move(question), std::move(colon), std::move(condition.value()), std::move(true_branch.value()), std::move(else_branch.value()) });
		}
		else
		{
			return std::unexpected<std::string>(GenerateParserError("Expected ':' for ternary expression.", question));
		}
	}

	return condition;
}

MidoriResult::ExpressionResult Parser::ParseExpression()
{
	return ParseAs();
}

MidoriResult::ExpressionResult Parser::ParseAs()
{
	MidoriResult::ExpressionResult expr = ParseBind();
	VERIFY_RESULT(expr);

	if (Match(Token::Name::AS))
	{
		Token& as = Previous();
		MidoriResult::TypeResult type = ParseType();
		VERIFY_RESULT(type);

		return std::make_unique<MidoriExpression>(As{ std::move(as), std::move(expr.value()), std::move(type.value()) });
	}

	return expr;
}

MidoriResult::ExpressionResult Parser::ParseArrayAccessHelper(std::unique_ptr<MidoriExpression>&& arr_var)
{
	Token& op = Previous();
	std::vector<std::unique_ptr<MidoriExpression>> indices;

	while (Match(Token::Name::LEFT_BRACKET))
	{
		MidoriResult::ExpressionResult binding = ParseBind();
		VERIFY_RESULT(binding);

		indices.emplace_back(std::move(binding.value()));
		Consume(Token::Name::RIGHT_BRACKET, "Expected ']' after index.");
	}

	return std::make_unique<MidoriExpression>(ArrayGet{ std::move(op), std::move(arr_var), std::move(indices) });
}

MidoriResult::ExpressionResult Parser::ParseArrayAccess()
{
	MidoriResult::ExpressionResult arr_var = ParsePrimary();
	VERIFY_RESULT(arr_var);

	if (Check(Token::Name::LEFT_BRACKET, 0))
	{
		return ParseArrayAccessHelper(std::move(arr_var.value()));
	}

	return arr_var;
}

MidoriResult::ExpressionResult Parser::ParseCall()
{
	MidoriResult::ExpressionResult expr = ParseArrayAccess();
	VERIFY_RESULT(expr);

	while (true)
	{
		if (Match(Token::Name::LEFT_PAREN))
		{
			expr = FinishCall(std::move(expr.value()));
			VERIFY_RESULT(expr);
		}
		else if (Match(Token::Name::DOT))
		{
			MidoriResult::TokenResult name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected identifier after '.'.");
			VERIFY_RESULT(name);

			expr = std::make_unique<MidoriExpression>(Get{ std::move(name.value()), std::move(expr.value()) });
		}
		else
		{
			break;
		}
	}

	return expr;
}

MidoriResult::ExpressionResult Parser::ParseConstruct()
{
	if (Match(Token::Name::NEW))
	{
		MidoriResult::TokenResult data_name_token = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected type name after 'new'.");
		VERIFY_RESULT(data_name_token);

		Token& data_name_token_value = data_name_token.value();
		std::optional<const MidoriType*> defined_type = std::nullopt;
		bool is_struct = false;
		Scopes::const_reverse_iterator scopes_iter = m_scopes.crbegin();
		for (; scopes_iter != m_scopes.crend(); ++scopes_iter)
		{
			const Scope& scope = *scopes_iter;
			if (scope.m_variables.contains(data_name_token_value.m_lexeme) && !scope.m_structs.contains(data_name_token_value.m_lexeme))
			{
				// variable overshadowing struct
				if (scope.m_unions.contains(data_name_token_value.m_lexeme))
				{
					defined_type.emplace(scope.m_unions.at(data_name_token_value.m_lexeme));
					break;
				}
				else
				{
					return std::unexpected<std::string>(GenerateParserError("Cannot construct type '" + data_name_token_value.m_lexeme + "' because it is overshadowed by a variable with the same name.", data_name_token_value));
				}
			}
			else if (scope.m_structs.contains(data_name_token_value.m_lexeme))
			{
				is_struct = true;
				defined_type.emplace(scope.m_structs.at(data_name_token_value.m_lexeme));
				break;
			}
		}

		if (defined_type == std::nullopt)
		{
			return std::unexpected<std::string>(GenerateParserError("Undefined struct.", data_name_token.value()));
		}

		MidoriResult::TokenResult paren = Consume(Token::Name::LEFT_PAREN, "Expected '(' after type.");
		if (!paren.has_value())
		{
			return std::unexpected<std::string>(std::move(paren.error()));
		}

		std::vector<std::unique_ptr<MidoriExpression>> arguments;
		if (!Check(Token::Name::RIGHT_PAREN, 0))
		{
			do
			{
				MidoriResult::ExpressionResult expr = ParseExpression();
				VERIFY_RESULT(expr);

				arguments.emplace_back(std::move(expr.value()));
			} while (Match(Token::Name::COMMA));
		}

		paren = Consume(Token::Name::RIGHT_PAREN, "Expected ')' after arguments.");
		VERIFY_RESULT(paren);

		if (is_struct)
		{
			return std::make_unique<MidoriExpression>(Construct{ std::move(data_name_token_value), std::move(arguments), defined_type.value(), Construct::Struct{} });
		}
		else
		{
			const UnionType& union_type = MidoriTypeUtil::GetUnionType(defined_type.value());
			return std::make_unique<MidoriExpression>(Construct{ data_name_token_value, std::move(arguments), defined_type.value(), Construct::Union{union_type.m_member_info.at(data_name_token_value.m_lexeme).m_tag} });
		}
	}
	else
	{
		return ParseCall();
	}
}

MidoriResult::ExpressionResult Parser::FinishCall(std::unique_ptr<MidoriExpression>&& callee)
{
	std::vector<std::unique_ptr<MidoriExpression>> arguments;
	if (!Check(Token::Name::RIGHT_PAREN, 0))
	{
		do
		{
			MidoriResult::ExpressionResult expr = ParseExpression();
			VERIFY_RESULT(expr);

			arguments.emplace_back(std::move(expr.value()));
		} while (Match(Token::Name::COMMA));
	}

	std::expected<Token, std::string> paren = Consume(Token::Name::RIGHT_PAREN, "Expected ')' after arguments.");
	return paren.has_value() ? MidoriResult::ExpressionResult(std::make_unique<MidoriExpression>(Call{ std::move(paren.value()), std::move(callee), std::move(arguments) })) : std::unexpected<std::string>(paren.error());
}

MidoriResult::ExpressionResult Parser::ParsePrimary()
{
	if (Match(Token::Name::LEFT_PAREN))
	{
		if (Match(Token::Name::RIGHT_PAREN))
		{
			return std::make_unique<MidoriExpression>(UnitLiteral{ std::move(Previous()) });
		}

		MidoriResult::ExpressionResult expr_in = ParseExpression();
		VERIFY_RESULT(expr_in);

		MidoriResult::TokenResult paren = Consume(Token::Name::RIGHT_PAREN, "Expected right parentheses.");
		VERIFY_RESULT(paren);

		return std::make_unique<MidoriExpression>(Group{ std::move(expr_in.value()) });
	}
	else if (Match(Token::Name::IDENTIFIER_LITERAL))
	{
		Token& variable = Previous();

		std::vector<Scope>::const_reverse_iterator found_scope_it = std::find_if(m_scopes.crbegin(), m_scopes.crend(), [&variable](const Scope& scope)
			{
				return scope.m_variables.contains(variable.m_lexeme);
			});

		if (found_scope_it != m_scopes.rend())
		{
			Scope::VariableTable::const_iterator find_result = found_scope_it->m_variables.find(variable.m_lexeme);

			if (IsGlobalName(found_scope_it))
			{
				return std::make_unique<MidoriExpression>(Variable{ std::move(Previous()), VariableSemantic::Global() });
			}
			else if (IsLocalName(find_result))
			{
				return std::make_unique<MidoriExpression>(Variable{ std::move(Previous()), VariableSemantic::Local(find_result->second.m_relative_index.value()) });
			}
			// cell
			else
			{
				return std::make_unique<MidoriExpression>(Variable{ std::move(Previous()), VariableSemantic::Cell(find_result->second.m_absolute_index.value()) });
			}
		}

		return std::unexpected<std::string>(GenerateParserError("Undefined Variable.", variable));
	}
	else if (Match(Token::Name::CLOSURE))
	{
		Token& keyword = Previous();

		MidoriResult::TokenResult paren = Consume(Token::Name::LEFT_PAREN, "Expected '(' before function parameters.");
		VERIFY_RESULT(paren);

		std::vector<Token> params;
		std::vector<const MidoriType*> param_types;

		int prev_total_locals = m_total_locals_in_curr_scope;
		m_total_locals_in_curr_scope = 0;
		m_closure_depth += 1;

		std::vector<std::unique_ptr<MidoriStatement>> statements;
		BeginScope();

		if (!Match(Token::Name::RIGHT_PAREN))
		{
			do
			{
				bool is_fixed = false;
				if (Match(Token::Name::VAR))
				{
					is_fixed = false;
				}
				else if (Match(Token::Name::FIXED))
				{
					is_fixed = true;
				}
				else
				{
					return std::unexpected<std::string>(std::unexpected<std::string>(GenerateParserError("Expected access modifier before parameter name.", Peek(0))));
				}

				MidoriResult::TokenResult name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected parameter name.");
				VERIFY_RESULT(name);

				VERIFY_RESULT(Consume(Token::Name::SINGLE_COLON, "Expected ':' before parameter type token."));

				MidoriResult::TypeResult param_type = ParseType();
				VERIFY_RESULT(param_type);

				MidoriResult::TokenResult param_name = DefineName(name.value(), is_fixed);
				if (param_name.has_value())
				{
					params.emplace_back(param_name.value());
					param_types.emplace_back(std::move(param_type.value()));
					GetLocalVariableIndex(param_name.value().m_lexeme, is_fixed);
				}
				else
				{
					return std::unexpected<std::string>(std::move(param_name.error()));
				}
			} while (Match(Token::Name::COMMA));

			VERIFY_RESULT(Consume(Token::Name::RIGHT_PAREN, "Expected ')' before function parameters."));
		}

		VERIFY_RESULT(Consume(Token::Name::SINGLE_COLON, "Expected ':' before function range type token."));

		MidoriResult::TypeResult return_type = ParseType();
		VERIFY_RESULT(return_type);

		VERIFY_RESULT(Consume(Token::Name::LEFT_BRACE, "Expected '{' before function expression body."));

		while (!IsAtEnd() && !Check(Token::Name::RIGHT_BRACE, 0))
		{
			MidoriResult::StatementResult decl = ParseDeclaration();
			VERIFY_RESULT(decl);

			statements.emplace_back(std::move(decl.value()));
		}

		VERIFY_RESULT(Consume(Token::Name::RIGHT_BRACE, "Expected '}' function expression body."));

		Token& right_brace = Previous();
		int block_local_count = EndScope();

		std::unique_ptr<MidoriStatement> closure_body = std::make_unique<MidoriStatement>(Block{ std::move(right_brace), std::move(statements), block_local_count });
		if (!HasReturnStatement(*closure_body))
		{
			return std::unexpected<std::string>(GenerateParserError("function does not return in all paths.", keyword));
		}

		m_closure_depth -= 1;
		m_total_locals_in_curr_scope = prev_total_locals;

		return std::make_unique<MidoriExpression>(Closure{ std::move(keyword), std::move(params), std::move(param_types), std::move(closure_body), std::move(return_type.value()), m_total_variables });
	}
	else if (Match(Token::Name::TRUE, Token::Name::FALSE))
	{
		return std::make_unique<MidoriExpression>(BoolLiteral{ std::move(Previous()) });
	}
	else if (Match(Token::Name::FRACTION_LITERAL))
	{
		return std::make_unique<MidoriExpression>(FractionLiteral{ std::move(Previous()) });
	}
	else if (Match(Token::Name::INTEGER_LITERAL))
	{
		return std::make_unique<MidoriExpression>(IntegerLiteral{ std::move(Previous()) });
	}
	else if (Match(Token::Name::TEXT_LITERAL))
	{
		return std::make_unique<MidoriExpression>(TextLiteral{ std::move(Previous()) });
	}
	else if (Match(Token::Name::LEFT_BRACKET))
	{
		Token& op = Previous();
		std::vector<std::unique_ptr<MidoriExpression>> expr_vector;

		if (Match(Token::Name::RIGHT_BRACKET))
		{
			return std::make_unique<MidoriExpression>(Array{ std::move(op), std::move(expr_vector) });
		}

		do
		{
			MidoriResult::ExpressionResult expr = ParseExpression();
			VERIFY_RESULT(expr);

			expr_vector.emplace_back(std::move(expr.value()));
		} while (Match(Token::Name::COMMA));

		VERIFY_RESULT(Consume(Token::Name::RIGHT_BRACKET, "Expected ']' for array expression."));

		return std::make_unique<MidoriExpression>(Array{ std::move(op), std::move(expr_vector) });
	}
	else
	{
		return std::unexpected<std::string>(GenerateParserError("Expected expression.", Peek(0)));
	}
}

MidoriResult::ExpressionResult Parser::ParseLogicalAnd()
{
	MidoriResult::ExpressionResult expr = ParseBitwiseOr();
	VERIFY_RESULT(expr);

	while (Match(Token::Name::DOUBLE_AMPERSAND))
	{
		Token& op = Previous();
		MidoriResult::ExpressionResult right = ParseBitwiseOr();
		if (!right.has_value())
		{
			return std::unexpected<std::string>(std::move(right.error()));
		}

		expr = MidoriResult::ExpressionResult(std::make_unique<MidoriExpression>(Binary{ std::move(op), std::move(expr.value()), std::move(right.value()) }));
	}

	return expr;
}

MidoriResult::ExpressionResult Parser::ParseLogicalOr()
{
	MidoriResult::ExpressionResult expr = ParseLogicalAnd();
	VERIFY_RESULT(expr);

	while (Match(Token::Name::DOUBLE_BAR))
	{
		Token& op = Previous();
		MidoriResult::ExpressionResult right = ParseLogicalAnd();
		VERIFY_RESULT(right);

		expr = MidoriResult::ExpressionResult(std::make_unique<MidoriExpression>(Binary{ std::move(op), std::move(expr.value()), std::move(right.value()) }));
	}

	return expr;
}

MidoriResult::StatementResult Parser::ParseDeclaration()
{
	constexpr bool may_throw_error = false;
	return ParseDeclarationCommon(may_throw_error);
}

MidoriResult::StatementResult Parser::ParseGlobalDeclaration()
{
	constexpr bool may_throw_error = true;
	return ParseDeclarationCommon(may_throw_error);
}

MidoriResult::StatementResult Parser::ParseBlockStatement()
{
	std::vector<std::unique_ptr<MidoriStatement>> statements;
	BeginScope();

	while (!IsAtEnd() && !Check(Token::Name::RIGHT_BRACE, 0))
	{
		MidoriResult::StatementResult decl = ParseDeclaration();
		VERIFY_RESULT(decl);

		statements.emplace_back(std::move(decl.value()));
	}

	VERIFY_RESULT(Consume(Token::Name::RIGHT_BRACE, "Expected '}' after block."));

	Token& right_brace = Previous();
	int block_local_count = EndScope();

	return std::make_unique<MidoriStatement>(Block{ std::move(right_brace), std::move(statements), std::move(block_local_count) });
}

MidoriResult::StatementResult Parser::ParseDefineStatement()
{
	bool is_fixed = Previous().m_token_name == Token::Name::FIXED;
	MidoriResult::TokenResult var_name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected variable name.");
	VERIFY_RESULT(var_name);

	MidoriResult::TokenResult define_name_result = DefineName(var_name.value(), is_fixed);
	VERIFY_RESULT(define_name_result);

	std::optional<const MidoriType*> type_annotation = std::nullopt;
	if (Match(Token::Name::SINGLE_COLON))
	{
		MidoriResult::TypeResult type_result = ParseType();
		VERIFY_RESULT(type_result);

		type_annotation.emplace(type_result.value());
	}

	Token& name = define_name_result.value();

	std::optional<int> local_index = GetLocalVariableIndex(name.m_lexeme, is_fixed);

	Consume(Token::Name::SINGLE_EQUAL, "Expected '=' after defining a name.");

	MidoriResult::ExpressionResult expr = ParseExpression();
	VERIFY_RESULT(expr);

	MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after variable declaration.");
	VERIFY_RESULT(semi_colon);

	return std::make_unique<MidoriStatement>(Define{ std::move(name), std::move(expr.value()), std::move(type_annotation), std::move(local_index) });
}

MidoriResult::StatementResult Parser::ParseStructDeclaration()
{
	MidoriResult::TokenResult name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected struct name.");
	VERIFY_RESULT(name);

	if (name.value().m_lexeme[0] != std::toupper(name.value().m_lexeme[0]))
	{
		return std::unexpected<std::string>(GenerateParserError("Struct name must start with a capital letter.", name.value()));
	}

	constexpr bool is_fixed = true;
	VERIFY_RESULT(DefineName(name.value(), is_fixed));

	VERIFY_RESULT(Consume(Token::Name::LEFT_BRACE, "Expected '{' before struct body."));

	std::vector<const MidoriType*> member_types;
	std::vector<std::string> member_names;

	while (!IsAtEnd())
	{
		if (Match(Token::Name::RIGHT_BRACE))
		{
			break;
		}

		MidoriResult::TokenResult identifier = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected struct member name.");
		VERIFY_RESULT(identifier);

		VERIFY_RESULT(Consume(Token::Name::SINGLE_COLON, "Expected ':' before struct member type token."));

		MidoriResult::TypeResult type = ParseType();
		VERIFY_RESULT(type);
		
		if (MidoriTypeUtil::IsStructType(type.value()) && (MidoriTypeUtil::GetStructType(type.value()).m_name == name.value().m_lexeme))
		{
			return std::unexpected<std::string>(GenerateParserError("Recursive struct is not allowed.", identifier.value()));
		}

		member_types.emplace_back(type.value());
		member_names.emplace_back(identifier.value().m_lexeme);

		if (Match(Token::Name::COMMA))
		{
			continue;
		}
		else if (Match(Token::Name::RIGHT_BRACE))
		{
			break;
		}
		else
		{
			return std::unexpected<std::string>(GenerateParserError("Expected ',' or '}' after struct member.", Peek(0)));
		}
	}

	const MidoriType* struct_type = MidoriTypeUtil::InsertStructType(name.value().m_lexeme, std::move(member_types), std::move(member_names));
	m_scopes.back().m_structs[name.value().m_lexeme] = struct_type;
	m_scopes.back().m_defined_types[name.value().m_lexeme] = struct_type;

	VERIFY_RESULT(Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after struct body."));

	return std::make_unique<MidoriStatement>(Struct{ std::move(name.value()), struct_type });
}

MidoriResult::StatementResult Parser::ParseUnionDeclaration()
{
	MidoriResult::TokenResult name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected union name.");
	VERIFY_RESULT(name);
	
	if (name.value().m_lexeme[0] != std::toupper(name.value().m_lexeme[0]))
	{
		return std::unexpected<std::string>(GenerateParserError("Union name must start with a capital letter.", name.value()));
	}

	int tag = 0;
	constexpr bool is_fixed = true;
	VERIFY_RESULT(DefineName(name.value(), is_fixed));

	const MidoriType* union_type = MidoriTypeUtil::InsertUnionType(name.value().m_lexeme);
	UnionType& union_type_ref = const_cast<UnionType&>(MidoriTypeUtil::GetUnionType(union_type));
	m_scopes.back().m_defined_types[name.value().m_lexeme] = union_type;

	VERIFY_RESULT(Consume(Token::Name::LEFT_BRACE, "Expected '{' before union body."));

	while (!IsAtEnd())
	{
		if (Match(Token::Name::RIGHT_BRACE))
		{
			break;
		}

		MidoriResult::TokenResult member_name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected union member name.");
		VERIFY_RESULT(member_name);

		VERIFY_RESULT(DefineName(member_name.value(), is_fixed));

		std::vector<const MidoriType*> member_type;
		if (Match(Token::Name::LEFT_PAREN))
		{
			do
			{
				MidoriResult::TypeResult type_result = ParseType();
				VERIFY_RESULT(type_result);

				member_type.emplace_back(type_result.value());
			} while (Match(Token::Name::COMMA));

			VERIFY_RESULT(Consume(Token::Name::RIGHT_PAREN, "Expected right parenthesis after union constructor."));
		}

		union_type_ref.m_member_info.emplace(member_name.value().m_lexeme, UnionType::UnionMemberContext{ member_type, tag });
		tag += 1;

		if (Match(Token::Name::COMMA))
		{
			continue;
		}
		else if (Match(Token::Name::RIGHT_BRACE))
		{
			break;
		}
		else
		{
			return std::unexpected<std::string>(GenerateParserError("Expected ',' or '}' after union member.", Peek(0)));
		}
	}

	std::ranges::for_each(union_type_ref.m_member_info, [union_type, this](const auto& member_info_entry)
		{
			m_scopes.back().m_unions[member_info_entry.first] = union_type;
		});

	VERIFY_RESULT(Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after union body."));

	return std::make_unique<MidoriStatement>(Union{ std::move(name.value()), union_type });
}

MidoriResult::StatementResult Parser::ParseIfStatement()
{
	Token& if_token = Previous();

	VERIFY_RESULT(Consume(Token::Name::LEFT_PAREN, "Expected '(' after \"if\"."));

	MidoriResult::ExpressionResult condition = ParseExpression();
	VERIFY_RESULT(condition);

	VERIFY_RESULT(Consume(Token::Name::RIGHT_PAREN, "Expected ')' after if condition."));

	MidoriResult::StatementResult then_branch = ParseStatement();
	VERIFY_RESULT(then_branch);

	std::optional<std::unique_ptr<MidoriStatement>> else_branch = std::nullopt;
	std::optional<Token> else_token = std::nullopt;
	if (Match(Token::Name::ELSE))
	{
		else_token.emplace(Previous());

		MidoriResult::StatementResult else_branch_result = ParseStatement();
		VERIFY_RESULT(else_branch_result);

		else_branch.emplace(std::move(else_branch_result.value()));
	}

	return std::make_unique<MidoriStatement>(If{ std::move(if_token), std::move(else_token), std::move(else_branch), std::move(condition.value()), std::move(then_branch.value()) });
}

MidoriResult::StatementResult Parser::ParseWhileStatement()
{
	Token& keyword = Previous();
	m_local_count_before_loop.emplace(m_total_variables);

	VERIFY_RESULT(Consume(Token::Name::LEFT_PAREN, "Expected '(' after \"while\"."));

	MidoriResult::ExpressionResult condition = ParseExpression();
	VERIFY_RESULT(condition);

	VERIFY_RESULT(Consume(Token::Name::RIGHT_PAREN, "Expected ')' after condition."));

	MidoriResult::StatementResult body = ParseStatement();
	VERIFY_RESULT(body);

	m_local_count_before_loop.pop();
	return std::make_unique<MidoriStatement>(While{ std::move(keyword), std::move(condition.value()), std::move(body.value()) });
}

MidoriResult::StatementResult Parser::ParseForStatement()
{
	Token& keyword = Previous();
	m_local_count_before_loop.emplace(m_total_variables);

	BeginScope();

	VERIFY_RESULT(Consume(Token::Name::LEFT_PAREN, "Expected '(' after \"for\"."));

	std::optional<std::unique_ptr<MidoriStatement>> initializer = std::nullopt;
	if (Match(Token::Name::VAR, Token::Name::FIXED))
	{
		MidoriResult::StatementResult init_result = ParseDefineStatement();
		VERIFY_RESULT(init_result);

		initializer.emplace(std::move(init_result.value()));
	}
	else if (!Match(Token::Name::SINGLE_SEMICOLON))
	{
		MidoriResult::StatementResult init_result = ParseSimpleStatement();
		VERIFY_RESULT(init_result);

		initializer.emplace(std::move(init_result.value()));
	}

	std::optional<std::unique_ptr<MidoriExpression>> condition = std::nullopt;
	if (!Check(Token::Name::SINGLE_SEMICOLON, 0))
	{
		MidoriResult::ExpressionResult expr = ParseExpression();
		VERIFY_RESULT(expr);

		condition.emplace(std::move(expr.value()));
	}
	MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after \"for\" condition.");
	VERIFY_RESULT(semi_colon);

	std::optional<std::unique_ptr<MidoriStatement>> increment = std::nullopt;
	if (!Check(Token::Name::RIGHT_PAREN, 0))
	{
		MidoriResult::ExpressionResult expr = ParseExpression();
		VERIFY_RESULT(expr);

		increment.emplace(std::make_unique<MidoriStatement>(Simple{ std::move(semi_colon.value()), std::move(expr.value()) }));
	}
	VERIFY_RESULT(Consume(Token::Name::RIGHT_PAREN, "Expected ')' after \"for\" clauses."));

	MidoriResult::StatementResult body = ParseStatement();
	VERIFY_RESULT(body);

	int control_block_local_count = EndScope();
	m_local_count_before_loop.pop();

	return std::make_unique<MidoriStatement>(For{ std::move(keyword), std::move(condition), std::move(increment), std::move(initializer), std::move(body.value()), control_block_local_count });
}

MidoriResult::StatementResult Parser::ParseBreakStatement()
{
	Token& keyword = Previous();

	if (m_local_count_before_loop.empty())
	{
		return std::unexpected<std::string>(GenerateParserError("'break' must be used inside a loop.", keyword));
	}

	VERIFY_RESULT(Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after \"break\"."));

	return std::make_unique<MidoriStatement>(Break{ std::move(keyword), m_total_variables - m_local_count_before_loop.top() });
}

MidoriResult::StatementResult Parser::ParseContinueStatement()
{
	Token& keyword = Previous();

	if (m_local_count_before_loop.empty())
	{
		return std::unexpected<std::string>(GenerateParserError("'continue' must be used inside a loop.", keyword));
	}

	VERIFY_RESULT(Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after \"continue\"."));

	return std::make_unique<MidoriStatement>(Continue{ std::move(keyword), m_total_variables - m_local_count_before_loop.top() - 1 });
}

MidoriResult::StatementResult Parser::ParseSimpleStatement()
{
	MidoriResult::ExpressionResult expr = ParseExpression();
	VERIFY_RESULT(expr);

	MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after expression.");
	VERIFY_RESULT(semi_colon);

	return std::make_unique<MidoriStatement>(Simple{ std::move(semi_colon.value()), std::move(expr.value()) });
}

MidoriResult::StatementResult Parser::ParseReturnStatement()
{
	Token& keyword = Previous();
	if (m_closure_depth == 0)
	{
		return std::unexpected<std::string>(GenerateParserError("'return' must be used inside a closure.", keyword));
	}

	MidoriResult::ExpressionResult expr = ParseExpression();
	VERIFY_RESULT(expr);

	VERIFY_RESULT(Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after return value."));

	return std::make_unique<MidoriStatement>(Return{ std::move(keyword), std::move(expr.value()) });
}

MidoriResult::StatementResult Parser::ParseForeignStatement()
{
	MidoriResult::TokenResult name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected foreign function name.");
	VERIFY_RESULT(name);

	VERIFY_RESULT(Consume(Token::Name::SINGLE_COLON, "Expected ':' before foreign function type."));

	constexpr bool is_fixed = true;
	name = DefineName(name.value(), is_fixed);
	VERIFY_RESULT(name);

	std::optional<int> local_index = GetLocalVariableIndex(name.value().m_lexeme, is_fixed);

	constexpr bool is_foreign = true;
	MidoriResult::TypeResult type = ParseType(is_foreign);
	VERIFY_RESULT(type);

	if (!MidoriTypeUtil::IsFunctionType(type.value()))
	{
		return std::unexpected<std::string>(GenerateParserError("'foreign' only applies to function types.", name.value()));
	}

	VERIFY_RESULT(Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after foreign function type."));

	return std::make_unique<MidoriStatement>(Foreign{ std::move(name.value()), std::move(type.value()), std::move(local_index) });
}

MidoriResult::StatementResult Parser::ParseSwitchStatement()
{
	Token& switch_keyword = Previous();
	VERIFY_RESULT(Consume(Token::Name::LEFT_PAREN, "Expected '(' before match statement."));

	MidoriResult::ExpressionResult expr = ParseExpression();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	VERIFY_RESULT(Consume(Token::Name::RIGHT_PAREN, "Expected ')' after match statement."));

	VERIFY_RESULT(Consume(Token::Name::LEFT_BRACE, "Expected '{' before cases."));

	std::unordered_set<std::string> visited_members;
	std::vector<Switch::Case> cases;
	bool has_default = false;
	while (Match(Token::Name::CASE) || Match(Token::Name::DEFAULT))
	{
		Token& keyword = Previous();
		if (keyword.m_token_name == Token::Name::CASE)
		{
			MidoriResult::TokenResult member_name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected constructor name.");
			VERIFY_RESULT(member_name);

			if (visited_members.contains(member_name.value().m_lexeme))
			{
				return std::unexpected<std::string>(GenerateParserError("Duplicate case in match statement.", member_name.value()));
			}
			else
			{
				visited_members.emplace(member_name.value().m_lexeme);
			}

			BeginScope();
			std::vector<std::string> binding_names;
			if (Match(Token::Name::LEFT_PAREN))
			{
				if (!Match(Token::Name::RIGHT_PAREN))
				{
					do
					{
						bool is_fixed = false;
						if (Match(Token::Name::FIXED))
						{
							is_fixed = true;
						}
						else if (Match(Token::Name::VAR))
						{
							is_fixed = false;
						}
						else
						{
							return std::unexpected<std::string>(GenerateParserError("Expected 'var' or 'fixed'.", Previous()));
						}

						MidoriResult::TokenResult field_name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected field name.");
						VERIFY_RESULT(field_name);

						field_name = DefineName(field_name.value(), is_fixed);
						VERIFY_RESULT(field_name);

						GetLocalVariableIndex(field_name.value().m_lexeme, is_fixed);

						binding_names.emplace_back(std::move(field_name.value().m_lexeme));
					} while (Match(Token::Name::COMMA));

					VERIFY_RESULT(Consume(Token::Name::RIGHT_PAREN, "Expected ')' after constructor name."));
				}
			}

			VERIFY_RESULT(Consume(Token::Name::SINGLE_COLON, "Expected ':' after case."));

			MidoriResult::StatementResult case_stmt = ParseStatement();
			VERIFY_RESULT(case_stmt);

			EndScope();

			cases.emplace_back(Switch::MemberCase{ std::move(keyword), std::move(binding_names), std::move(member_name.value().m_lexeme), std::move(case_stmt.value()) });
		}
		else
		{
			if (has_default)
			{
				return std::unexpected<std::string>(GenerateParserError("Cannot have more than one default case.", Previous()));
			}
			else
			{
				has_default = true;
			}

			VERIFY_RESULT(Consume(Token::Name::SINGLE_COLON, "Expected ':' after default case."));

			MidoriResult::StatementResult case_stmt = ParseStatement();
			VERIFY_RESULT(case_stmt);

			cases.emplace_back(Switch::DefaultCase{ std::move(keyword), std::move(case_stmt.value()) });
		}
	}

	VERIFY_RESULT(Consume(Token::Name::RIGHT_BRACE, "Expected '}' after cases."));

	return std::make_unique<MidoriStatement>(::Switch{ std::move(switch_keyword), std::move(expr.value()), std::move(cases) });
}


MidoriResult::StatementResult Parser::ParseStatement()
{
	if (Match(Token::Name::LEFT_BRACE))
	{
		return ParseBlockStatement();
	}
	else if (Match(Token::Name::IF))
	{
		return ParseIfStatement();
	}
	else if (Match(Token::Name::WHILE))
	{
		return ParseWhileStatement();
	}
	else if (Match(Token::Name::FOR))
	{
		return ParseForStatement();
	}
	else if (Match(Token::Name::BREAK))
	{
		return ParseBreakStatement();
	}
	else if (Match(Token::Name::CONTINUE))
	{
		return ParseContinueStatement();
	}
	else if (Match(Token::Name::RETURN))
	{
		return ParseReturnStatement();
	}
	else if (Match(Token::Name::SWITCH))
	{
		return ParseSwitchStatement();
	}
	else
	{
		return ParseSimpleStatement();
	}
}

MidoriResult::TypeResult Parser::ParseType(bool is_foreign)
{
	if (Match(Token::Name::TEXT))
	{
		return MidoriTypeUtil::GetType("Text"s);
	}
	else if (Match(Token::Name::FRACTION))
	{
		return MidoriTypeUtil::GetType("Frac"s);
	}
	else if (Match(Token::Name::INTEGER))
	{
		return MidoriTypeUtil::GetType("Int"s);
	}
	else if (Match(Token::Name::BOOL))
	{
		return MidoriTypeUtil::GetType("Bool"s);
	}
	else if (Match(Token::Name::UNIT))
	{
		return MidoriTypeUtil::GetType("Unit"s);
	}
	else if (Match(Token::Name::ARRAY))
	{
		VERIFY_RESULT(Consume(Token::Name::LEFT_BRACKET, "Expected '[' after 'array'."));

		MidoriResult::TypeResult type = ParseType();
		VERIFY_RESULT(type);

		VERIFY_RESULT(Consume(Token::Name::RIGHT_BRACKET, "Expected ']' after array type."));

		return MidoriTypeUtil::InsertArrayType(type.value());
	}
	else if (Match(Token::Name::LEFT_PAREN))
	{
		std::vector<const MidoriType*> types;
		if (!Match(Token::Name::RIGHT_PAREN))
		{
			do
			{
				MidoriResult::TypeResult type = ParseType();
				VERIFY_RESULT(type);

				types.emplace_back(type.value());
			} while (Match(Token::Name::COMMA));

			Consume(Token::Name::RIGHT_PAREN, "Expected ')' after argument types.");
		}
		Consume(Token::Name::THIN_ARROW, "Expected '->' before return type token.");

		MidoriResult::TypeResult return_type = ParseType();
		VERIFY_RESULT(return_type);

		return MidoriTypeUtil::InsertFunctionType(std::move(types), return_type.value(), is_foreign);
	}
	else if (Match(Token::Name::IDENTIFIER_LITERAL))
	{
		Token& type_name = Previous();

		std::vector<Scope>::const_reverse_iterator found_scope_it = std::find_if(m_scopes.crbegin(), m_scopes.crend(), [&type_name](const Scope& scope)
			{
				return scope.m_defined_types.contains(type_name.m_lexeme);
			});

		if (found_scope_it == m_scopes.crend())
		{
			return std::unexpected<std::string>(GenerateParserError("Undefined struct or union.", type_name));
		}

		return found_scope_it->m_defined_types.at(type_name.m_lexeme);
	}
	else
	{
		return std::unexpected<std::string>(GenerateParserError("Expected type token.", Peek(0)));
	}
}

bool Parser::HasCircularDependency() const
{
	std::unordered_set<std::string> visited;
	std::unordered_set<std::string> in_progress;
	std::queue<std::string> queue;

	queue.emplace(m_file_name);
	in_progress.emplace(m_file_name);

	while (!queue.empty())
	{
		std::string current = queue.front();
		queue.pop();
		in_progress.erase(current);
		visited.emplace(current);

		for (const std::string& dependency : s_dependency_graph[current])
		{
			if (visited.contains(dependency))
			{
				continue; // Already visited, skip
			}
			if (in_progress.contains(dependency))
			{
				return true; // Cycle detected
			}
			queue.emplace(dependency);
			in_progress.insert(dependency);
		}
	}

	return false;
}

MidoriResult::TokenResult Parser::HandleDirective()
{
	Token& directive = Previous();
	if (directive.m_lexeme == "include"s)
	{
		VERIFY_RESULT(Consume(Token::Name::TEXT_LITERAL, "Expected text literal after include directive."));
		Token& include_path = Previous();
		std::string include_absolute_path_str = std::filesystem::absolute(include_path.m_lexeme).string();

		if (s_dependency_graph.contains(include_absolute_path_str))
		{
			return directive;
		}

		s_dependency_graph[m_file_name].emplace_back(include_absolute_path_str);

		std::ifstream include_file(include_absolute_path_str);
		if (!include_file.is_open())
		{
			return std::unexpected<std::string>(GenerateParserError("Could not open include file.", include_path));
		}

		if (HasCircularDependency())
		{
			return std::unexpected<std::string>(GenerateParserError("Circular dependency detected.", include_path));
		}

		std::ostringstream include_file_stream;
		include_file_stream << include_file.rdbuf();

		constexpr bool is_main_program = false;
		Lexer include_lexer(include_file_stream.str(), is_main_program);
		MidoriResult::LexerResult include_lexer_result = include_lexer.Lex();

		if (!include_lexer_result.has_value())
		{
			return std::unexpected<std::string>(include_lexer_result.error());
		}

		m_tokens.Erase(m_tokens.begin() + m_current_token_index);
		m_tokens.Insert(m_tokens.begin(), std::move(include_lexer_result.value()));
		m_current_token_index = 0;
		return directive;
	}
	else
	{
		return std::unexpected<std::string>(GenerateParserError("Unknown directive '" + directive.m_lexeme + "'.", directive));
	}
}

bool Parser::HasReturnStatement(const MidoriStatement& stmt)
{
	return std::visit([this](auto&& arg) -> bool
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, Return>)
			{
				return true;
			}
			else if constexpr (std::is_same_v<T, If>)
			{
				bool true_branch_result = HasReturnStatement(*arg.m_true_branch);

				if (!true_branch_result)
				{
					return false;
				}

				if (arg.m_else_branch.has_value())
				{
					return true_branch_result && HasReturnStatement(*arg.m_else_branch.value());
				}

				return false;
			}
			else if constexpr (std::is_same_v<T, Block>)
			{
				for (const std::unique_ptr<MidoriStatement>& statement : arg.m_stmts)
				{
					if (HasReturnStatement(*statement))
					{
						return true;
					}
				}
				return false;
			}
			else if constexpr (std::is_same_v<T, For>)
			{
				return HasReturnStatement(*arg.m_body);
			}
			else if constexpr (std::is_same_v<T, While>)
			{
				return HasReturnStatement(*arg.m_body);
			}
			else
			{
				return false;
			}
		}, stmt);
}

MidoriResult::StatementResult Parser::ParseDeclarationCommon(bool may_throw_error)
{
	if (Match(Token::Name::VAR, Token::Name::FIXED))
	{
		return ParseDefineStatement();
	}
	else if (Match(Token::Name::STRUCT))
	{
		return ParseStructDeclaration();
	}
	else if (Match(Token::Name::UNION))
	{
		return ParseUnionDeclaration();
	}
	else if (Match(Token::Name::FOREIGN))
	{
		return ParseForeignStatement();
	}

	if (may_throw_error)
	{
		return std::unexpected<std::string>(GenerateParserError("Expected declaration.", Peek(0)));
	}
	return ParseStatement();
}

MidoriResult::ParserResult Parser::Parse()
{
	MidoriProgramTree programTree;
	std::string errors;

	BeginScope();
	while (!IsAtEnd())
	{
		while (Match(Token::Name::DIRECTIVE))
		{
			MidoriResult::TokenResult directive_result = HandleDirective();
			VERIFY_RESULT(directive_result);
		}

		MidoriResult::StatementResult result = ParseGlobalDeclaration();
		if (result.has_value())
		{
			programTree.emplace_back(std::move(result.value()));
		}
		else
		{
			errors.append(result.error());
			errors.push_back('\n');
		}
	}
	EndScope();

	if (errors.empty())
	{
		return programTree;
	}
	else
	{
		return std::unexpected<std::string>(std::move(errors));
	}
}

void Parser::Synchronize()
{
	Advance();

	while (!IsAtEnd())
	{
		if (IsAtGlobalScope())
		{
			switch (Peek(0).m_token_name)
			{
			case Token::Name::VAR:
			case Token::Name::FIXED:
			case Token::Name::STRUCT:
				return;
			default:
				break;
			}
		}

		Advance();
	}
}