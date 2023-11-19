#include "Parser.h"

#include <algorithm>

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
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	if (Match(Token::Name::SINGLE_EQUAL))
	{
		Token& equal = Previous();
		MidoriResult::ExpressionResult value = ParseBind();
		if (!value.has_value())
		{
			return std::unexpected<std::string>(std::move(value.error()));
		}

		if (std::holds_alternative<Variable>(*expr.value()))
		{
			Variable& variable_expr = std::get<Variable>(*expr.value());
			// native function
			if (m_native_functions.find(variable_expr.m_name.m_lexeme) != m_native_functions.end())
			{
				return std::unexpected<std::string>(GenerateParserError("Cannot bind to a native function.", variable_expr.m_name));
			}

			std::vector<Scope>::reverse_iterator found_scope_it = std::find_if(m_scopes.rbegin(), m_scopes.rend(), [&variable_expr](const Scope& scope)
				{
					return scope.find(variable_expr.m_name.m_lexeme) != scope.end();
				});

			if (found_scope_it != m_scopes.rend())
			{
				Scope::const_iterator find_result = found_scope_it->find(variable_expr.m_name.m_lexeme);

				if (find_result->second.m_is_fixed)
				{
					return std::unexpected<std::string>(GenerateParserError("Cannot break a fixed name binding.", variable_expr.m_name));
				}

				// global
				if (found_scope_it == std::prev(m_scopes.rend()))
				{
					return std::make_unique<Expression>(Bind(std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Global()));
				}
				// local
				else if (m_closure_depth == 0 || find_result->second.m_closure_depth == m_closure_depth)
				{
					return std::make_unique<Expression>(Bind(std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Local(find_result->second.m_relative_index)));
				}
				// cell
				else
				{
					return std::make_unique<Expression>(Bind(std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Cell(find_result->second.m_absolute_index)));
				}
			}

			return std::unexpected<std::string>(GenerateParserError("Undefined Variable.", variable_expr.m_name));
		}
		else if (std::holds_alternative<Get>(*expr.value()))
		{
			Get& get_expr = std::get<Get>(*expr.value());
			return std::make_unique<Expression>(Set(std::move(get_expr.m_name), std::move(get_expr.m_object), std::move(value.value())));
		}
		else if (std::holds_alternative<ArrayGet>(*expr.value()))
		{
			ArrayGet& access_expr = std::get<ArrayGet>(*expr.value());
			return std::make_unique<Expression>(ArraySet(std::move(access_expr.m_op), std::move(access_expr.m_arr_var), std::move(access_expr.m_indices), std::move(value.value())));
		}

		return std::unexpected<std::string>(GenerateParserError("Invalid binding target.", equal));
	}

	return expr;
}

MidoriResult::ExpressionResult Parser::ParseUnary()
{
	if (Match(Token::Name::BANG, Token::Name::MINUS))
	{
		Token& op = Previous();
		MidoriResult::ExpressionResult right = ParseUnary();
		if (!right.has_value())
		{
			return std::unexpected<std::string>(std::move(right.error()));
		}

		return std::make_unique<Expression>(Unary(std::move(op), std::move(right.value())));
	}

	return ParseCall();
}

MidoriResult::ExpressionResult Parser::ParseTernary()
{
	MidoriResult::ExpressionResult condition = ParseLogicalOr();
	if (!condition.has_value())
	{
		return std::unexpected<std::string>(std::move(condition.error()));
	}

	if (Match(Token::Name::QUESTION))
	{
		Token& question = Previous();
		MidoriResult::ExpressionResult true_branch = ParseTernary();
		if (!true_branch.has_value())
		{
			return std::unexpected<std::string>(std::move(true_branch.error()));
		}

		if (Match(Token::Name::SINGLE_COLON))
		{
			Token& colon = Previous();
			MidoriResult::ExpressionResult else_branch = ParseTernary();
			if (!else_branch.has_value())
			{
				return std::unexpected<std::string>(std::move(else_branch.error()));
			}

			return std::make_unique<Expression>(Ternary(std::move(question), std::move(colon), std::move(condition.value()), std::move(true_branch.value()), std::move(else_branch.value())));
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
	return ParseBind();
}

MidoriResult::ExpressionResult Parser::ParseArrayAccessHelper(std::unique_ptr<Expression>&& arr_var)
{
	Token& op = Previous();
	std::vector<std::unique_ptr<Expression>> indices;

	while (Match(Token::Name::LEFT_BRACKET))
	{
		MidoriResult::ExpressionResult binding = ParseBind();
		if (!binding.has_value())
		{
			return std::unexpected<std::string>(std::move(binding.error()));
		}

		indices.emplace_back(std::move(binding.value()));
		Consume(Token::Name::RIGHT_BRACKET, "Expected ']' after index.");
	}

	return std::make_unique<Expression>(ArrayGet(std::move(op), std::move(arr_var), std::move(indices)));
}

MidoriResult::ExpressionResult Parser::ParseArrayAccess()
{
	MidoriResult::ExpressionResult arr_var = ParsePrimary();
	if (!arr_var.has_value())
	{
		return std::unexpected<std::string>(std::move(arr_var.error()));
	}

	if (Check(Token::Name::LEFT_BRACKET, 0))
	{
		return ParseArrayAccessHelper(std::move(arr_var.value()));
	}

	return arr_var;
}

MidoriResult::ExpressionResult Parser::ParseCall()
{
	MidoriResult::ExpressionResult expr = ParseArrayAccess();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	while (true)
	{
		if (Match(Token::Name::LEFT_PAREN))
		{
			expr = FinishCall(std::move(expr.value()));
			if (!expr.has_value())
			{
				return std::unexpected<std::string>(std::move(expr.error()));
			}
		}
		else if (Match(Token::Name::DOT))
		{
			MidoriResult::TokenResult name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected identifier after '.'.");
			if (!name.has_value())
			{
				return std::unexpected<std::string>(std::move(name.error()));
			}

			expr = std::make_unique<Expression>(Get(std::move(name.value()), std::move(expr.value())));
		}
		else
		{
			break;
		}
	}

	return expr;
}

MidoriResult::ExpressionResult Parser::FinishCall(std::unique_ptr<Expression>&& callee)
{
	std::vector<std::unique_ptr<Expression>> arguments;
	if (!Check(Token::Name::RIGHT_PAREN, 0))
	{
		do
		{
			MidoriResult::ExpressionResult expr = ParseExpression();
			if (!expr.has_value())
			{
				return std::unexpected<std::string>(std::move(expr.error()));
			}

			arguments.emplace_back(std::move(expr.value()));
		} while (Match(Token::Name::COMMA));
	}

	std::expected<Token, std::string> paren = Consume(Token::Name::RIGHT_PAREN, "Expected ')' after arguments.");
	return paren.has_value() ? MidoriResult::ExpressionResult(std::make_unique<Expression>(Call(std::move(paren.value()), std::move(callee), std::move(arguments)))) : std::unexpected<std::string>(paren.error());
}

MidoriResult::ExpressionResult Parser::ParsePrimary()
{
	if (Match(Token::Name::LEFT_PAREN))
	{
		MidoriResult::ExpressionResult expr_in = ParseExpression();
		if (!expr_in.has_value())
		{
			return std::unexpected<std::string>(std::move(expr_in.error()));
		}

		MidoriResult::TokenResult paren = Consume(Token::Name::RIGHT_PAREN, "Expected right parentheses.");
		if (!paren.has_value())
		{
			return std::unexpected<std::string>(std::move(paren.error()));
		}

		return std::make_unique<Expression>(Group(std::move(expr_in.value())));
	}
	else if (Match(Token::Name::IDENTIFIER_LITERAL))
	{
		Token& variable = Previous();

		if (m_native_functions.find(variable.m_lexeme) != m_native_functions.end())
		{
			return std::make_unique<Expression>(Variable(std::move(Previous()), VariableSemantic::Global()));
		}

		std::vector<Scope>::reverse_iterator found_scope_it = std::find_if(m_scopes.rbegin(), m_scopes.rend(), [&variable](const Scope& scope)
			{
				return scope.find(variable.m_lexeme) != scope.end();
			});

		if (found_scope_it != m_scopes.rend())
		{
			Scope::const_iterator find_result = found_scope_it->find(variable.m_lexeme);

			// global
			if (found_scope_it == std::prev(m_scopes.rend()))
			{
				return std::make_unique<Expression>(Variable(std::move(Previous()), VariableSemantic::Global()));
			}
			// local
			else if (m_closure_depth == 0 || find_result->second.m_closure_depth == m_closure_depth)
			{
				return std::make_unique<Expression>(Variable(std::move(Previous()), VariableSemantic::Local(find_result->second.m_relative_index)));
			}
			// cell
			else
			{
				return std::make_unique<Expression>(Variable(std::move(Previous()), VariableSemantic::Cell(find_result->second.m_absolute_index)));
			}
		}

		return std::unexpected<std::string>(GenerateParserError("Undefined Variable.", variable));
	}
	else if (Match(Token::Name::CLOSURE))
	{
		Token& keyword = Previous();

		Consume(Token::Name::LEFT_PAREN, "Expected '(' before closure parameters.");
		std::vector<Token> params;
		std::vector<std::shared_ptr<MidoriType>> param_types;

		int prev_total_locals = m_total_locals_in_curr_scope;
		m_total_locals_in_curr_scope = 0;
		m_closure_depth += 1;

		std::vector<std::unique_ptr<Statement>> statements;
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
					return std::unexpected<std::string>(std::unexpected<std::string>(GenerateParserError("Expected access modifier before parameter name.", Previous())));
				}

				MidoriResult::TokenResult name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected parameter name.");
				if (!name.has_value())
				{
					return std::unexpected<std::string>(std::move(name.error()));
				}

				MidoriResult::TokenResult colon = Consume(Token::Name::SINGLE_COLON, "Expected ':' before parameter type.");
				if (!colon.has_value())
				{
					return std::unexpected<std::string>(std::move(colon.error()));
				}

				MidoriResult::TypeResult param_type = ParseType();
				if (!param_type.has_value())
				{
					return std::unexpected<std::string>(std::move(param_type.error()));
				}

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

			MidoriResult::TokenResult paren = Consume(Token::Name::RIGHT_PAREN, "Expected ')' before closure parameters.");
			if (!paren.has_value())
			{
				return std::unexpected<std::string>(std::move(paren.error()));
			}
		}

		MidoriResult::TokenResult arrow = Consume(Token::Name::SINGLE_COLON, "Expected ':' before closure range type.");
		if (!arrow.has_value())
		{
			return std::unexpected<std::string>(std::move(arrow.error()));
		}

		MidoriResult::TypeResult return_type = ParseType();
		if (!return_type.has_value())
		{
			return std::unexpected<std::string>(std::move(return_type.error()));
		}

		MidoriResult::TokenResult brace = Consume(Token::Name::LEFT_BRACE, "Expected '{' before closure expression body.");
		if (!brace.has_value())
		{
			return std::unexpected<std::string>(std::move(brace.error()));
		}

		while (!IsAtEnd() && !Check(Token::Name::RIGHT_BRACE, 0))
		{
			MidoriResult::StatementResult decl = ParseDeclaration();
			if (!decl.has_value())
			{
				return std::unexpected<std::string>(std::move(decl.error()));
			}

			statements.emplace_back(std::move(decl.value()));
		}

		brace = Consume(Token::Name::RIGHT_BRACE, "Expected '}' closure expression body.");
		if (!brace.has_value())
		{
			return std::unexpected<std::string>(std::move(brace.error()));
		}

		Token& right_brace = Previous();
		int block_local_count = EndScope();

		std::unique_ptr<Statement> closure_body = std::make_unique<Statement>(Block(std::move(right_brace), std::move(statements), block_local_count));
		if (!HasReturnStatement(*closure_body))
		{
			return std::unexpected<std::string>(GenerateParserError("Closure does not return in all paths.", keyword));
		}

		m_closure_depth -= 1;
		m_total_locals_in_curr_scope = prev_total_locals;

		return std::make_unique<Expression>(Closure(std::move(keyword), std::move(params), std::move(param_types), std::move(closure_body), std::move(return_type.value()), m_total_variables));
	}
	else if (Match(Token::Name::TRUE, Token::Name::FALSE))
	{
		return std::make_unique<Expression>(BoolLiteral(std::move(Previous())));
	}
	else if (Match(Token::Name::HASH))
	{
		return std::make_unique<Expression>(UnitLiteral(std::move(Previous())));
	}
	else if (Match(Token::Name::FRACTION_LITERAL))
	{
		return std::make_unique<Expression>(FractionLiteral(std::move(Previous())));
	}
	else if (Match(Token::Name::INTEGER_LITERAL))
	{
		return std::make_unique<Expression>(IntegerLiteral(std::move(Previous())));
	}
	else if (Match(Token::Name::TEXT_LITERAL))
	{
		return std::make_unique<Expression>(TextLiteral(std::move(Previous())));
	}
	else if (Match(Token::Name::LEFT_BRACKET))
	{
		Token& op = Previous();
		std::vector<std::unique_ptr<Expression>> expr_vector;

		if (Match(Token::Name::RIGHT_BRACKET))
		{
			return std::unexpected<std::string>(GenerateParserError("Cannot initialize an empty array.", Previous()));
			//return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::nullopt));
		}

		do
		{
			MidoriResult::ExpressionResult expr = ParseExpression();
			if (!expr.has_value())
			{
				return std::unexpected<std::string>(std::move(expr.error()));
			}

			expr_vector.emplace_back(std::move(expr.value()));
		} while (Match(Token::Name::COMMA));

		MidoriResult::TokenResult bracket = Consume(Token::Name::RIGHT_BRACKET, "Expected ']' for array expression.");
		if (!bracket.has_value())
		{
			return std::unexpected<std::string>(std::move(bracket.error()));
		}

		return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector)));
	}
	else
	{
		return std::unexpected<std::string>(GenerateParserError("Expected expression.", Previous()));
	}
}

MidoriResult::ExpressionResult Parser::ParseLogicalAnd()
{
	MidoriResult::ExpressionResult expr = ParseBitwiseOr();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	while (Match(Token::Name::DOUBLE_AMPERSAND))
	{
		Token& op = Previous();
		MidoriResult::ExpressionResult right = ParseBitwiseOr();
		if (!right.has_value())
		{
			return std::unexpected<std::string>(std::move(right.error()));
		}

		expr = MidoriResult::ExpressionResult(std::make_unique<Expression>(Binary(std::move(op), std::move(expr.value()), std::move(right.value()))));
	}

	return expr;
}

MidoriResult::ExpressionResult Parser::ParseLogicalOr()
{
	MidoriResult::ExpressionResult expr = ParseLogicalAnd();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	while (Match(Token::Name::DOUBLE_BAR))
	{
		Token& op = Previous();
		MidoriResult::ExpressionResult right = ParseLogicalAnd();
		if (!right.has_value())
		{
			return std::unexpected<std::string>(std::move(right.error()));
		}

		expr = MidoriResult::ExpressionResult(std::make_unique<Expression>(Binary(std::move(op), std::move(expr.value()), std::move(right.value()))));
	}

	return expr;
}

MidoriResult::StatementResult Parser::ParseDeclaration()
{
	if (Match(Token::Name::VAR, Token::Name::FIXED))
	{
		return ParseDefineStatement();
	}
	else if (Match(Token::Name::NAMESPACE))
	{
		return std::unexpected<std::string>(GenerateParserError("Namespace is not yet implemented!!!", Peek(0)));
		//return ParseNamespaceDeclaration();
	}
	return ParseStatement();
}

MidoriResult::StatementResult Parser::ParseDeclarationHelper()
{
	if (Match(Token::Name::VAR, Token::Name::FIXED))
	{
		return ParseDefineStatement();
	}
	else if (Match(Token::Name::NAMESPACE))
	{
		return std::unexpected<std::string>(GenerateParserError("Namespace is not yet implemented!!!", Peek(0)));
		//return ParseNamespaceDeclaration();
	}
	return std::unexpected<std::string>(GenerateParserError("Expected declaration.", Peek(0)));
}

MidoriResult::StatementResult Parser::ParseBlockStatement()
{
	std::vector<std::unique_ptr<Statement>> statements;
	BeginScope();

	while (!IsAtEnd() && !Check(Token::Name::RIGHT_BRACE, 0))
	{
		MidoriResult::StatementResult decl = ParseDeclaration();
		if (!decl.has_value())
		{
			return std::unexpected<std::string>(std::move(decl.error()));
		}
		else
		{
			statements.emplace_back(std::move(decl.value()));
		}
	}

	MidoriResult::TokenResult brace = Consume(Token::Name::RIGHT_BRACE, "Expected '}' after block.");
	if (!brace.has_value())
	{
		return std::unexpected<std::string>(std::move(brace.error()));
	}

	Token& right_brace = Previous();
	int block_local_count = EndScope();

	return std::make_unique<Statement>(Block(std::move(right_brace), std::move(statements), std::move(block_local_count)));
}

MidoriResult::StatementResult Parser::ParseDefineStatement()
{
	bool is_fixed = Previous().m_token_type == Token::Name::FIXED;
	MidoriResult::TokenResult var_name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected variable name.");
	if (!var_name.has_value())
	{
		return std::unexpected<std::string>(std::move(var_name.error()));
	}

	MidoriResult::TokenResult define_name_result = DefineName(var_name.value(), is_fixed);
	if (!define_name_result.has_value())
	{
		return std::unexpected<std::string>(std::move(define_name_result.error()));
	}

	std::optional<std::shared_ptr<MidoriType>> type_annotation = std::nullopt;
	if (Match(Token::Name::SINGLE_COLON))
	{
		MidoriResult::TypeResult type_result = ParseType();
		if (!type_result.has_value())
		{
			return std::unexpected<std::string>(std::move(type_result.error()));
		}
		else
		{
			type_annotation.emplace(type_result.value());
		}
	}

	Token& name = define_name_result.value();

	std::optional<int> local_index = GetLocalVariableIndex(name.m_lexeme, is_fixed);

	Consume(Token::Name::COLON_EQUAL, "Expected ':=' after defining a name.");

	MidoriResult::ExpressionResult expr = ParseExpression();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}
	else
	{
		MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after variable declaration.");
		if (!semi_colon.has_value())
		{
			return std::unexpected<std::string>(std::move(semi_colon.error()));
		}
		else
		{
			return std::make_unique<Statement>(Define(std::move(name), std::move(expr.value()), std::move(type_annotation), std::move(local_index)));
		}
	}
}

MidoriResult::StatementResult Parser::ParseNamespaceDeclaration()
{
	MidoriResult::TokenResult name = Consume(Token::Name::IDENTIFIER_LITERAL, "Expected module name.");
	if (!name.has_value())
	{
		return std::unexpected<std::string>(std::move(name.error()));
	}

	MidoriResult::TokenResult brace = Consume(Token::Name::LEFT_BRACE, "Expected '{' before module body.");
	if (!brace.has_value())
	{
		return std::unexpected<std::string>(std::move(brace.error()));
	}

	MidoriResult::StatementResult block = ParseBlockStatement();
	if (!block.has_value())
	{
		return std::unexpected<std::string>(std::move(block.error()));
	}
	return std::make_unique<Statement>(Namespace(std::move(name.value()), std::move(block.value())));
}

MidoriResult::StatementResult Parser::ParseIfStatement()
{
	Token& if_token = Previous();

	MidoriResult::TokenResult paren = Consume(Token::Name::LEFT_PAREN, "Expected '(' after \"if\".");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	MidoriResult::ExpressionResult condition = ParseExpression();
	if (!condition.has_value())
	{
		return std::unexpected<std::string>(std::move(condition.error()));
	}

	paren = Consume(Token::Name::RIGHT_PAREN, "Expected ')' after if condition.");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	MidoriResult::StatementResult then_branch = ParseStatement();
	if (!then_branch.has_value())
	{
		return std::unexpected<std::string>(std::move(then_branch.error()));
	}

	std::optional<std::unique_ptr<Statement>> else_branch = std::nullopt;
	std::optional<Token> else_token = std::nullopt;
	if (Match(Token::Name::ELSE))
	{
		else_token.emplace(Previous());

		MidoriResult::StatementResult else_branch_result = ParseStatement();
		if (!else_branch_result.has_value())
		{
			return std::unexpected<std::string>(std::move(else_branch_result.error()));
		}
		else_branch.emplace(std::move(else_branch_result.value()));
	}

	return std::make_unique<Statement>(If(std::move(if_token), std::move(else_token), std::move(else_branch), std::move(condition.value()), std::move(then_branch.value())));
}

MidoriResult::StatementResult Parser::ParseWhileStatement()
{
	Token& keyword = Previous();
	m_local_count_before_loop.emplace(m_total_variables);

	MidoriResult::TokenResult paren = Consume(Token::Name::LEFT_PAREN, "Expected '(' after \"while\".");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	MidoriResult::ExpressionResult condition = ParseExpression();
	if (!condition.has_value())
	{
		return std::unexpected<std::string>(std::move(condition.error()));
	}

	paren = Consume(Token::Name::RIGHT_PAREN, "Expected ')' after condition.");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	MidoriResult::StatementResult body = ParseStatement();
	if (!body.has_value())
	{
		return std::unexpected<std::string>(std::move(body.error()));
	}

	m_local_count_before_loop.pop();
	return std::make_unique<Statement>(While(std::move(keyword), std::move(condition.value()), std::move(body.value())));
}

MidoriResult::StatementResult Parser::ParseForStatement()
{
	Token& keyword = Previous();
	m_local_count_before_loop.emplace(m_total_variables);

	BeginScope();

	MidoriResult::TokenResult paren = Consume(Token::Name::LEFT_PAREN, "Expected '(' after \"for\".");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	std::optional<std::unique_ptr<Statement>> initializer = std::nullopt;
	if (Match(Token::Name::VAR, Token::Name::FIXED))
	{
		MidoriResult::StatementResult init_result = ParseDefineStatement();
		if (!init_result.has_value())
		{
			return std::unexpected<std::string>(std::move(init_result.error()));
		}
		else
		{
			initializer.emplace(std::move(init_result.value()));
		}
	}
	else if (!Match(Token::Name::SINGLE_SEMICOLON))
	{
		MidoriResult::StatementResult init_result = ParseSimpleStatement();
		if (!init_result.has_value())
		{
			return std::unexpected<std::string>(std::move(init_result.error()));
		}
		else
		{
			initializer.emplace(std::move(init_result.value()));
		}
	}

	std::optional<std::unique_ptr<Expression>> condition = std::nullopt;
	if (!Check(Token::Name::SINGLE_SEMICOLON, 0))
	{
		MidoriResult::ExpressionResult expr = ParseExpression();
		if (!expr.has_value())
		{
			return std::unexpected<std::string>(std::move(expr.error()));
		}
		else
		{
			condition.emplace(std::move(expr.value()));
		}
	}
	MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after \"for\" condition.");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	std::optional<std::unique_ptr<Statement>> increment = std::nullopt;
	if (!Check(Token::Name::RIGHT_PAREN, 0))
	{
		MidoriResult::ExpressionResult expr = ParseExpression();
		if (!expr.has_value())
		{
			return std::unexpected<std::string>(std::move(expr.error()));
		}
		else
		{
			increment.emplace(std::make_unique<Statement>(Simple(std::move(semi_colon.value()), std::move(expr.value()))));
		}
	}
	paren = Consume(Token::Name::RIGHT_PAREN, "Expected ')' after \"for\" clauses.");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	MidoriResult::StatementResult body = ParseStatement();
	if (!body.has_value())
	{
		return std::unexpected<std::string>(std::move(body.error()));
	}

	int control_block_local_count = EndScope();
	m_local_count_before_loop.pop();

	body = std::make_unique<Statement>(For(std::move(keyword), std::move(condition), std::move(increment), std::move(initializer), std::move(body.value()), control_block_local_count));

	return body;
}

MidoriResult::StatementResult Parser::ParseBreakStatement()
{
	Token& keyword = Previous();

	if (m_local_count_before_loop.empty())
	{
		return std::unexpected<std::string>(GenerateParserError("'break' must be used inside a loop.", keyword));
	}

	MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after \"break\".");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Break(std::move(keyword), m_total_variables - m_local_count_before_loop.top()));
}

MidoriResult::StatementResult Parser::ParseContinueStatement()
{
	Token& keyword = Previous();

	if (m_local_count_before_loop.empty())
	{
		return std::unexpected<std::string>(GenerateParserError("'continue' must be used inside a loop.", keyword));
	}

	MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after \"continue\".");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Continue(std::move(keyword), m_total_variables - m_local_count_before_loop.top() - 1));
}

MidoriResult::StatementResult Parser::ParseSimpleStatement()
{
	MidoriResult::ExpressionResult expr = ParseExpression();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after expression.");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Simple(std::move(semi_colon.value()), std::move(expr.value())));
}

MidoriResult::StatementResult Parser::ParseReturnStatement()
{
	Token& keyword = Previous();
	if (m_closure_depth == 0)
	{
		return std::unexpected<std::string>(GenerateParserError("'return' must be used inside a closure.", keyword));
	}

	MidoriResult::ExpressionResult expr = ParseExpression();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	MidoriResult::TokenResult semi_colon = Consume(Token::Name::SINGLE_SEMICOLON, "Expected ';' after return value.");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Return(std::move(keyword), std::move(expr.value())));
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
	else
	{
		return ParseSimpleStatement();
	}
}

MidoriResult::TypeResult Parser::ParseType()
{
	if (Match(Token::Name::TEXT))
	{
		return std::make_shared<MidoriType>(TextType());
	}
	else if (Match(Token::Name::FRACTION))
	{
		return std::make_shared<MidoriType>(FractionType());
	}
	else if (Match(Token::Name::INTEGER))
	{
		return std::make_shared<MidoriType>(IntegerType());
	}
	else if (Match(Token::Name::BOOL))
	{
		return std::make_shared<MidoriType>(BoolType());
	}
	else if (Match(Token::Name::UNIT))
	{
		return std::make_shared<MidoriType>(UnitType());
	}
	else if (Match(Token::Name::ARRAY))
	{
		Consume(Token::Name::LEFT_ANGLE, "Expected '<' after array type.");
		MidoriResult::TypeResult type = ParseType();
		if (!type.has_value())
		{
			return std::unexpected<std::string>(std::move(type.error()));
		}
		else
		{
			Consume(Token::Name::RIGHT_ANGLE, "Expected '>' after array type.");
			return std::make_shared<MidoriType>(ArrayType(std::move(type.value())));
		}
	}
	else if (Match(Token::Name::LEFT_PAREN))
	{
		std::vector<std::shared_ptr<MidoriType>> types;
		if (!Match(Token::Name::RIGHT_PAREN))
		{
			do
			{
				MidoriResult::TypeResult type = ParseType();
				if (!type.has_value())
				{
					return std::unexpected<std::string>(std::move(type.error()));
				}
				else
				{
					types.emplace_back(type.value());
				}
			} while (Match(Token::Name::COMMA));

			Consume(Token::Name::RIGHT_PAREN, "Expected ')' after argument types.");
		}
		Consume(Token::Name::THIN_ARROW, "Expected '->' before return type.");

		MidoriResult::TypeResult return_type = ParseType();

		if (!return_type.has_value())
		{
			return std::unexpected<std::string>(std::move(return_type.error()));
		}
		else
		{
			return std::make_shared<MidoriType>(FunctionType(std::move(types), std::move(return_type.value())));
		}
	}
	else
	{
		return std::unexpected<std::string>(GenerateParserError("Expected type.", Peek(0)));
	}
}

bool Parser::HasReturnStatement(const Statement& stmt)
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
				for (const std::unique_ptr<Statement>& statement : arg.m_stmts)
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

MidoriResult::ParserResult Parser::Parse()
{
	ProgramTree programTree;
	std::vector<std::string> errors;

	BeginScope();
	while (!IsAtEnd())
	{
		MidoriResult::StatementResult result = ParseDeclarationHelper();
		if (result.has_value())
		{
			programTree.emplace_back(std::move(result.value()));
		}
		else
		{
			errors.emplace_back(result.error());
		}
	}
	EndScope();

	if (errors.empty())
	{
		return programTree;
	}
	else
	{
		return std::unexpected<std::vector<std::string>>(std::move(errors));
	}
}

void Parser::Synchronize()
{
	Advance();

	while (!IsAtEnd())
	{
		if (Previous().m_token_type == Token::Name::SINGLE_SEMICOLON)
		{
			return;
		}
		switch (Peek(0).m_token_type)
		{
		case Token::Name::NAMESPACE:
		case Token::Name::VAR:
		case Token::Name::FIXED:
		case Token::Name::FOR:
		case Token::Name::IF:
		case Token::Name::WHILE:
		case Token::Name::RETURN:
			return;
		default:
			break;
		}

		Advance();
	}
}