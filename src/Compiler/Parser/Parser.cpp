#include "Parser.h"

#include <algorithm>

MidoriResult::ExpressionResult Parser::ParseFactor()
{
	return ParseBinary(&Parser::ParseUnary, Token::Type::STAR, Token::Type::SLASH, Token::Type::PERCENT);
}

MidoriResult::ExpressionResult Parser::ParseShift()
{
	return ParseBinary(&Parser::ParseTerm, Token::Type::LEFT_SHIFT, Token::Type::RIGHT_SHIFT);
}

MidoriResult::ExpressionResult Parser::ParseTerm()
{
	return ParseBinary(&Parser::ParseFactor, Token::Type::SINGLE_PLUS, Token::Type::DOUBLE_PLUS, Token::Type::MINUS);
}

MidoriResult::ExpressionResult Parser::ParseComparison()
{
	return ParseBinary(&Parser::ParseShift, Token::Type::LESS, Token::Type::LESS_EQUAL, Token::Type::GREATER, Token::Type::GREATER_EQUAL);
}

MidoriResult::ExpressionResult Parser::ParseEquality()
{
	return ParseBinary(&Parser::ParseComparison, Token::Type::BANG_EQUAL, Token::Type::DOUBLE_EQUAL);
}

MidoriResult::ExpressionResult Parser::ParseBitwiseAnd()
{
	return ParseBinary(&Parser::ParseEquality, Token::Type::SINGLE_AMPERSAND);
}

MidoriResult::ExpressionResult Parser::ParseBitwiseXor()
{
	return ParseBinary(&Parser::ParseBitwiseAnd, Token::Type::CARET);
}

MidoriResult::ExpressionResult Parser::ParseBitwiseOr()
{
	return ParseBinary(&Parser::ParseBitwiseXor, Token::Type::SINGLE_BAR);
}

MidoriResult::ExpressionResult Parser::ParseAssignment()
{
	MidoriResult::ExpressionResult expr = ParseTernary();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	if (Match(Token::Type::SINGLE_EQUAL))
	{
		Token& equal = Previous();
		MidoriResult::ExpressionResult value = ParseAssignment();
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
				return std::unexpected<std::string>(GenerateParserError("Cannot assign to a native function.", variable_expr.m_name));
			}

			for (std::vector<Scope>::reverse_iterator scope_it = m_scopes.rbegin(); scope_it != m_scopes.rend(); ++scope_it)
			{
				Scope::const_iterator find_result = scope_it->find(variable_expr.m_name.m_lexeme);
				if (find_result != scope_it->end())
				{
					if (find_result->second.m_is_fixed)
					{
						return std::unexpected<std::string>(GenerateParserError("Cannot assign to a fixed name binding.", variable_expr.m_name));
					}

					// global 
					if (scope_it == std::prev(m_scopes.rend()))
					{
						return std::make_unique<Expression>(Assign(std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Global()));
					}
					// local
					else if (m_closure_depth == 0 || find_result->second.m_closure_depth == m_closure_depth)
					{
						return std::make_unique<Expression>(Assign(std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Local(find_result->second.m_relative_index)));
					}
					// cell
					else
					{
						return std::make_unique<Expression>(Assign(std::move(variable_expr.m_name), std::move(value.value()), VariableSemantic::Cell(find_result->second.m_absolute_index)));
					}
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

		return std::unexpected<std::string>(GenerateParserError("Invalid assignment target.", equal));
	}

	return expr;
}

MidoriResult::ExpressionResult Parser::ParseUnary()
{
	if (Match(Token::Type::BANG, Token::Type::MINUS))
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

	if (Match(Token::Type::QUESTION))
	{
		Token& question = Previous();
		MidoriResult::ExpressionResult true_branch = ParseTernary();
		if (!true_branch.has_value())
		{
			return std::unexpected<std::string>(std::move(true_branch.error()));
		}

		if (Match(Token::Type::SINGLE_COLON))
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
	return ParseAssignment();
}

MidoriResult::ExpressionResult Parser::ParseArrayAccessHelper(std::unique_ptr<Expression>&& arr_var)
{
	Token& op = Previous();
	std::vector<std::unique_ptr<Expression>> indices;

	while (Match(Token::Type::LEFT_BRACKET))
	{
		MidoriResult::ExpressionResult assignment = ParseAssignment();
		if (!assignment.has_value())
		{
			return std::unexpected<std::string>(std::move(assignment.error()));
		}

		indices.emplace_back(std::move(assignment.value()));
		Consume(Token::Type::RIGHT_BRACKET, "Expected ']' after index.");
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

	if (Check(Token::Type::LEFT_BRACKET, 0))
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
		if (Match(Token::Type::LEFT_PAREN))
		{
			expr = FinishCall(std::move(expr.value()));
			if (!expr.has_value())
			{
				return std::unexpected<std::string>(std::move(expr.error()));
			}
		}
		else if (Match(Token::Type::DOT))
		{
			MidoriResult::TokenResult name = Consume(Token::Type::IDENTIFIER, "Expected identifier after '.'.");
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
	if (!Check(Token::Type::RIGHT_PAREN, 0))
	{
		do
		{
			MidoriResult::ExpressionResult expr = ParseExpression();
			if (!expr.has_value())
			{
				return std::unexpected<std::string>(std::move(expr.error()));
			}

			arguments.emplace_back(std::move(expr.value()));
		} while (Match(Token::Type::COMMA));
	}

	std::expected<Token, std::string> paren = Consume(Token::Type::RIGHT_PAREN, "Expected ')' after arguments.");
	return paren.has_value() ? MidoriResult::ExpressionResult(std::make_unique<Expression>(Call(std::move(paren.value()), std::move(callee), std::move(arguments)))) : std::unexpected<std::string>(paren.error());
}

MidoriResult::ExpressionResult Parser::ParsePrimary()
{
	if (Match(Token::Type::LEFT_PAREN))
	{
		MidoriResult::ExpressionResult expr_in = ParseExpression();
		if (!expr_in.has_value())
		{
			return std::unexpected<std::string>(std::move(expr_in.error()));
		}

		MidoriResult::TokenResult paren = Consume(Token::Type::RIGHT_PAREN, "Expected right parentheses.");
		if (!paren.has_value())
		{
			return std::unexpected<std::string>(std::move(paren.error()));
		}

		return std::make_unique<Expression>(Group(std::move(expr_in.value())));
	}
	else if (Match(Token::Type::IDENTIFIER))
	{
		Token& variable = Previous();
		// native function
		if (m_native_functions.find(variable.m_lexeme) != m_native_functions.end())
		{
			return std::make_unique<Expression>(Variable(std::move(Previous()), VariableSemantic::Global()));
		}

		for (std::vector<Scope>::reverse_iterator scope_it = m_scopes.rbegin(); scope_it != m_scopes.rend(); ++scope_it)
		{
			Scope::const_iterator find_result = scope_it->find(variable.m_lexeme);
			if (find_result != scope_it->end())
			{
				// global 
				if (scope_it == std::prev(m_scopes.rend()))
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
		}

		return std::unexpected<std::string>(GenerateParserError("Undefined Variable.", variable));
	}
	else if (Match(Token::Type::CLOSURE))
	{
		Token& keyword = Previous();

		Consume(Token::Type::LEFT_PAREN, "Expected '(' before closure parameters.");
		std::vector<Token> params;

		int prev_total_locals = m_total_locals;
		m_total_locals = 0;
		m_closure_depth += 1;

		std::vector<std::unique_ptr<Statement>> statements;
		BeginScope();

		if (!Match(Token::Type::RIGHT_PAREN))
		{
			do
			{
				bool is_fixed = false;
				if (Match(Token::Type::VAR))
				{
					is_fixed = false;
				}
				else if (Match(Token::Type::FIXED))
				{
					is_fixed = true;
				}
				else
				{
					return std::unexpected<std::string>(std::unexpected<std::string>(GenerateParserError("Expected access modifier before parameter name.", Previous())));
				}

				MidoriResult::TokenResult name = Consume(Token::Type::IDENTIFIER, "Expected parameter name.");
				if (!name.has_value())
				{
					return std::unexpected<std::string>(std::move(name.error()));
				}

				MidoriResult::TokenResult param_name = DefineName(name.value(), is_fixed);
				if (param_name.has_value())
				{
					params.emplace_back(param_name.value());
					GetLocalVariableIndex(param_name.value().m_lexeme, is_fixed);
				}
				else
				{
					return std::unexpected<std::string>(std::move(param_name.error()));
				}
			} while (Match(Token::Type::COMMA));

			MidoriResult::TokenResult paren = Consume(Token::Type::RIGHT_PAREN, "Expected ')' before closure parameters.");
			if (!paren.has_value())
			{
				return std::unexpected<std::string>(std::move(paren.error()));
			}
		}
		MidoriResult::TokenResult brace = Consume(Token::Type::LEFT_BRACE, "Expected '{' before closure expression body.");
		if (!brace.has_value())
		{
			return std::unexpected<std::string>(std::move(brace.error()));
		}

		while (!IsAtEnd() && !Check(Token::Type::RIGHT_BRACE, 0))
		{
			MidoriResult::StatementResult decl = ParseDeclaration();
			if (!decl.has_value())
			{
				return std::unexpected<std::string>(std::move(decl.error()));
			}

			statements.emplace_back(std::move(decl.value()));
		}

		brace = Consume(Token::Type::RIGHT_BRACE, "Expected '}' closure expression body.");
		if (!brace.has_value())
		{
			return std::unexpected<std::string>(std::move(brace.error()));
		}

		Token& right_brace = Previous();
		int block_local_count = EndScope();

		std::unique_ptr<Statement> closure_body = std::make_unique<Statement>(Block(std::move(right_brace), std::move(statements), block_local_count));

		m_closure_depth -= 1;
		m_total_locals = prev_total_locals;

		return std::make_unique<Expression>(Closure(std::move(keyword), std::move(params), std::move(closure_body), std::make_unique<Type>(AnyType()), m_total_variables));
	}
	else if (Match(Token::Type::TRUE, Token::Type::FALSE))
	{
		return std::make_unique<Expression>(Bool(std::move(Previous())));
	}
	else if (Match(Token::Type::HASH))
	{
		return std::make_unique<Expression>(Unit(std::move(Previous())));
	}
	else if (Match(Token::Type::NUMBER))
	{
		return std::make_unique<Expression>(Number(std::move(Previous())));
	}
	else if (Match(Token::Type::STRING))
	{
		return std::make_unique<Expression>(String(std::move(Previous())));
	}
	else if (Match(Token::Type::LEFT_BRACKET))
	{
		Token& op = Previous();
		std::vector<std::unique_ptr<Expression>> expr_vector;

		if (Match(Token::Type::RIGHT_BRACKET))
		{
			return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::nullopt));
		}
		else if (Match(Token::Type::AT))
		{
			MidoriResult::ExpressionResult allocated_size = ParseTernary();
			if (!allocated_size.has_value())
			{
				return std::unexpected<std::string>(std::move(allocated_size.error()));
			}

			MidoriResult::TokenResult bracket = Consume(Token::Type::RIGHT_BRACKET, "Expected ']' for array expression.");
			if (!bracket.has_value())
			{
				return std::unexpected<std::string>(std::move(bracket.error()));
			}

			return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::move(allocated_size.value())));
		}

		do
		{
			MidoriResult::ExpressionResult expr = ParseExpression();
			if (!expr.has_value())
			{
				return std::unexpected<std::string>(std::move(expr.error()));
			}

			expr_vector.emplace_back(std::move(expr.value()));
		} while (Match(Token::Type::COMMA));

		MidoriResult::TokenResult bracket = Consume(Token::Type::RIGHT_BRACKET, "Expected ']' for array expression.");
		if (!bracket.has_value())
		{
			return std::unexpected<std::string>(std::move(bracket.error()));
		}

		return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::nullopt));
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

	while (Match(Token::Type::DOUBLE_AMPERSAND))
	{
		Token& op = Previous();
		MidoriResult::ExpressionResult right = ParseBitwiseOr();
		if (!right.has_value())
		{
			return std::unexpected<std::string>(std::move(right.error()));
		}

		expr = MidoriResult::ExpressionResult(std::make_unique<Expression>(Logical(std::move(op), std::move(expr.value()), std::move(right.value()))));
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

	while (Match(Token::Type::DOUBLE_BAR))
	{
		Token& op = Previous();
		MidoriResult::ExpressionResult right = ParseLogicalAnd();
		if (!right.has_value())
		{
			return std::unexpected<std::string>(std::move(right.error()));
		}

		expr = MidoriResult::ExpressionResult(std::make_unique<Expression>(Logical(std::move(op), std::move(expr.value()), std::move(right.value()))));
	}

	return expr;
}

MidoriResult::StatementResult Parser::ParseDeclaration()
{
	if (Match(Token::Type::VAR, Token::Type::FIXED))
	{
		return ParseDefineStatement();
	}
	else if (Match(Token::Type::NAMESPACE))
	{
		return std::unexpected<std::string>(GenerateParserError("Namespace is not yet implemented!!!", Peek(0)));
		//return ParseNamespaceDeclaration();
	}
	return ParseStatement();
}

MidoriResult::StatementResult Parser::ParseDeclarationHelper()
{
	if (Match(Token::Type::VAR, Token::Type::FIXED))
	{
		return ParseDefineStatement();
	}
	else if (Match(Token::Type::NAMESPACE))
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

	while (!IsAtEnd() && !Check(Token::Type::RIGHT_BRACE, 0))
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

	MidoriResult::TokenResult brace = Consume(Token::Type::RIGHT_BRACE, "Expected '}' after block.");
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
	bool is_fixed = Previous().m_token_type == Token::Type::FIXED;
	MidoriResult::TokenResult var_name = Consume(Token::Type::IDENTIFIER, "Expected variable name.");
	if (!var_name.has_value())
	{
		return std::unexpected<std::string>(std::move(var_name.error()));
	}

	MidoriResult::TokenResult define_name_result = DefineName(var_name.value(), is_fixed);
	if (!define_name_result.has_value())
	{
		return std::unexpected<std::string>(std::move(define_name_result.error()));
	}
	Token& name = define_name_result.value();

	std::optional<int> local_index = GetLocalVariableIndex(name.m_lexeme, is_fixed);

	if (Match(Token::Type::SINGLE_EQUAL))
	{
		MidoriResult::ExpressionResult expr = ParseExpression();
		if (!expr.has_value())
		{
			return std::unexpected<std::string>(std::move(expr.error()));
		}
		else
		{
			MidoriResult::TokenResult semi_colon = Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after variable declaration.");
			if (!semi_colon.has_value())
			{
				return std::unexpected<std::string>(std::move(semi_colon.error()));
			}
			else
			{
				return std::make_unique<Statement>(Define(std::move(name), std::move(expr.value()), std::move(local_index)));
			}
		}
	}
	else
	{
		return std::unexpected<std::string>(GenerateParserError("Expected initializer.", Previous()));
	}
}

MidoriResult::StatementResult Parser::ParseNamespaceDeclaration()
{
	MidoriResult::TokenResult name = Consume(Token::Type::IDENTIFIER, "Expected module name.");
	if (!name.has_value())
	{
		return std::unexpected<std::string>(std::move(name.error()));
	}

	MidoriResult::TokenResult brace = Consume(Token::Type::LEFT_BRACE, "Expected '{' before module body.");
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

	MidoriResult::TokenResult paren = Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"if\".");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	MidoriResult::ExpressionResult condition = ParseExpression();
	if (!condition.has_value())
	{
		return std::unexpected<std::string>(std::move(condition.error()));
	}

	paren = Consume(Token::Type::RIGHT_PAREN, "Expected ')' after if condition.");
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
	if (Match(Token::Type::ELSE))
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

	MidoriResult::TokenResult paren = Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"while\".");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	MidoriResult::ExpressionResult condition = ParseExpression();
	if (!condition.has_value())
	{
		return std::unexpected<std::string>(std::move(condition.error()));
	}

	paren = Consume(Token::Type::RIGHT_PAREN, "Expected ')' after condition.");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	MidoriResult::StatementResult body = ParseStatement();
	if (!body.has_value())
	{
		return std::unexpected<std::string>(std::move(body.error()));
	}

	return std::make_unique<Statement>(While(std::move(keyword), std::move(condition.value()), std::move(body.value())));
}

MidoriResult::StatementResult Parser::ParseForStatement()
{
	Token& keyword = Previous();

	BeginScope();

	MidoriResult::TokenResult paren = Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"for\".");
	if (!paren.has_value())
	{
		return std::unexpected<std::string>(std::move(paren.error()));
	}

	std::unique_ptr<Statement> initializer;
	if (Match(Token::Type::VAR, Token::Type::FIXED))
	{
		MidoriResult::StatementResult init_result = ParseDefineStatement();
		if (!init_result.has_value())
		{
			return std::unexpected<std::string>(std::move(init_result.error()));
		}
		else
		{
			initializer = std::move(init_result.value());
		}
	}
	else if (!Match(Token::Type::SINGLE_SEMICOLON))
	{
		MidoriResult::StatementResult init_result = ParseSimpleStatement();
		if (!init_result.has_value())
		{
			return std::unexpected<std::string>(std::move(init_result.error()));
		}
		else
		{
			initializer = std::move(init_result.value());
		}
	}

	std::optional<std::unique_ptr<Expression>> condition = std::nullopt;
	if (!Check(Token::Type::SINGLE_SEMICOLON, 0))
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
	MidoriResult::TokenResult semi_colon = Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"for\" condition.");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	std::optional<std::unique_ptr<Statement>> increment = std::nullopt;
	if (!Check(Token::Type::RIGHT_PAREN, 0))
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
	paren = Consume(Token::Type::RIGHT_PAREN, "Expected ')' after \"for\" clauses.");
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

	body = std::make_unique<Statement>(For(std::move(keyword), std::move(condition), std::move(increment), std::move(initializer), std::move(body.value()), control_block_local_count));

	return body;
}

MidoriResult::StatementResult Parser::ParseBreakStatement()
{
	Token& keyword = Previous();

	MidoriResult::TokenResult semi_colon = Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"break\".");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Break(std::move(keyword)));
}

MidoriResult::StatementResult Parser::ParseContinueStatement()
{
	Token& keyword = Previous();

	MidoriResult::TokenResult semi_colon = Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"continue\".");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Continue(std::move(keyword)));
}

MidoriResult::StatementResult Parser::ParseSimpleStatement()
{
	MidoriResult::ExpressionResult expr = ParseExpression();
	if (!expr.has_value())
	{
		return std::unexpected<std::string>(std::move(expr.error()));
	}

	MidoriResult::TokenResult semi_colon = Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after expression.");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Simple(std::move(semi_colon.value()), std::move(expr.value())));
}

MidoriResult::StatementResult Parser::ParseImportStatement()
{
	Token& keyword = Previous();
	MidoriResult::ExpressionResult path = ParseExpression();
	if (!path.has_value())
	{
		return std::unexpected<std::string>(std::move(path.error()));
	}

	MidoriResult::TokenResult semi_colon = Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after script path.");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Import(std::move(keyword), std::move(path.value())));
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

	MidoriResult::TokenResult semi_colon = Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after return value.");
	if (!semi_colon.has_value())
	{
		return std::unexpected<std::string>(std::move(semi_colon.error()));
	}

	return std::make_unique<Statement>(Return(std::move(keyword), std::move(expr.value())));
}


MidoriResult::StatementResult Parser::ParseStatement()
{
	if (Match(Token::Type::LEFT_BRACE))
	{
		return ParseBlockStatement();
	}
	else if (Match(Token::Type::IF))
	{
		return ParseIfStatement();
	}
	else if (Match(Token::Type::WHILE))
	{
		return ParseWhileStatement();
	}
	else if (Match(Token::Type::FOR))
	{
		return ParseForStatement();
	}
	else if (Match(Token::Type::BREAK))
	{
		return ParseBreakStatement();
	}
	else if (Match(Token::Type::CONTINUE))
	{
		return ParseContinueStatement();
	}
	else if (Match(Token::Type::RETURN))
	{
		return ParseReturnStatement();
	}
	else if (Match(Token::Type::IMPORT))
	{
		return ParseImportStatement();
	}
	else
	{
		return ParseSimpleStatement();
	}
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
		if (Previous().m_token_type == Token::Type::SINGLE_SEMICOLON)
		{
			return;
		}
		switch (Peek(0).m_token_type)
		{
		case Token::Type::NAMESPACE:
		case Token::Type::VAR:
		case Token::Type::FIXED:
		case Token::Type::FOR:
		case Token::Type::IF:
		case Token::Type::WHILE:
		case Token::Type::RETURN:
			return;
		default:
			break;
		}

		Advance();
	}
}