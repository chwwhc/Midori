#include "Parser.h"

#include <iostream>

std::unique_ptr<Expression> Parser::ParseFactor()
{
	return ParseBinary(&Parser::ParseUnary, Token::Type::STAR, Token::Type::SLASH, Token::Type::PERCENT);
}

std::unique_ptr<Expression> Parser::ParseShift()
{
	return ParseBinary(&Parser::ParseTerm, Token::Type::LEFT_SHIFT, Token::Type::RIGHT_SHIFT);
}

std::unique_ptr<Expression> Parser::ParseTerm()
{
	return ParseBinary(&Parser::ParseFactor, Token::Type::PLUS, Token::Type::MINUS);
}

std::unique_ptr<Expression> Parser::ParseComparison()
{
	return ParseBinary(&Parser::ParseShift, Token::Type::LESS, Token::Type::LESS_EQUAL, Token::Type::GREATER, Token::Type::GREATER_EQUAL);
}

std::unique_ptr<Expression> Parser::ParseEquality()
{
	return ParseBinary(&Parser::ParseComparison, Token::Type::BANG_EQUAL, Token::Type::DOUBLE_EQUAL);
}

std::unique_ptr<Expression> Parser::ParseBitwiseAnd()
{
	return ParseBinary(&Parser::ParseEquality, Token::Type::SINGLE_AMPERSAND);
}

std::unique_ptr<Expression> Parser::ParseBitwiseXor()
{
	return ParseBinary(&Parser::ParseBitwiseAnd, Token::Type::CARET);
}

std::unique_ptr<Expression> Parser::ParseBitwiseOr()
{
	return ParseBinary(&Parser::ParseBitwiseXor, Token::Type::SINGLE_BAR);
}

std::unique_ptr<Expression> Parser::ParseAssignment()
{
	std::unique_ptr<Expression> expr = ParseArrayLiteral();

	if (Match(Token::Type::SINGLE_EQUAL))
	{
		Token& equal = Previous();
		std::unique_ptr<Expression> value = ParseAssignment();

		if (std::holds_alternative<Variable>(*expr))
		{
			Variable& variable_expr = std::get<Variable>(*expr);

			for (std::vector<std::unordered_map<std::string, int>>::reverse_iterator scope_it = m_scopes.rbegin(); scope_it != m_scopes.rend(); ++scope_it)
			{
				std::unordered_map<std::string, int>::const_iterator find_result = scope_it->find(variable_expr.m_name.m_lexeme);
				if (find_result != scope_it->end())
				{
					// global
					if (scope_it == std::prev(m_scopes.rend()))
					{
						return std::make_unique<Expression>(Assign(std::move(variable_expr.m_name), std::move(value), std::nullopt));
					}
					// local
					else
					{
						return std::make_unique<Expression>(Assign(std::move(variable_expr.m_name), std::move(value), find_result->second));
					}
				}
			}

			CompilerError::PrintError(CompilerError::Type::PARSER, "Undefined Variable.", variable_expr.m_name);
			m_error = true;
			Synchronize();
			return nullptr;
		}
		else if (std::holds_alternative<Get>(*expr))
		{
			Get& get_expr = std::get<Get>(*expr);
			return std::make_unique<Expression>(Set(std::move(get_expr.m_name), std::move(get_expr.m_object), std::move(value)));
		}
		else if (std::holds_alternative<ArrayGet>(*expr))
		{
			ArrayGet& access_expr = std::get<ArrayGet>(*expr);
			return std::make_unique<Expression>(ArraySet(std::move(access_expr.m_op), std::move(access_expr.m_arr_var), std::move(access_expr.m_indices), std::move(value)));
		}

		CompilerError::PrintError(CompilerError::Type::PARSER, "Invalid assignment target.", equal);
		m_error = true;
		Synchronize();
	}

	return expr;
}

std::unique_ptr<Expression> Parser::ParseUnary()
{
	if (Match(Token::Type::BANG, Token::Type::MINUS))
	{
		Token& op = Previous();
		std::unique_ptr<Expression> right = ParseUnary();
		return std::make_unique<Expression>(Unary(std::move(op), std::move(right)));
	}

	return ParseArrayAccess();
}

std::unique_ptr<Expression> Parser::ParseTernary()
{
	std::unique_ptr<Expression> condition = ParseLogicalOr();

	if (Match(Token::Type::QUESTION))
	{
		Token& question = Previous();
		std::unique_ptr<Expression> true_branch = ParseTernary();
		if (Match(Token::Type::SINGLE_COLON))
		{
			Token& colon = Previous();
			std::unique_ptr<Expression> else_branch = ParseTernary();
			return std::make_unique<Expression>(Ternary(std::move(question), std::move(colon), std::move(condition), std::move(true_branch), std::move(else_branch)));
		}
		else
		{
			CompilerError::PrintError(CompilerError::Type::PARSER, "Expected ':' for ternary expression.", question);
			m_error = true;
			Synchronize();
		}
	}

	return condition;
}

std::unique_ptr<Expression> Parser::ParseExpression()
{
	return ParseAssignment();
}

std::unique_ptr<Expression> Parser::ParseArrayAccessHelper(std::unique_ptr<Expression>&& arr_var)
{
	Token& op = Previous();
	std::vector<std::unique_ptr<Expression>> indices;

	while (Match(Token::Type::LEFT_BRACKET))
	{
		indices.emplace_back(ParseAssignment());
		Consume(Token::Type::RIGHT_BRACKET, "Expected ']' after index.");
	}

	return std::make_unique<Expression>(ArrayGet(std::move(op), std::move(arr_var), std::move(indices)));
}

std::unique_ptr<Expression> Parser::ParseArrayAccess()
{
	std::unique_ptr<Expression> arr_var = ParseCall();

	if (Check(Token::Type::LEFT_BRACKET, 0))
	{
		return ParseArrayAccessHelper(std::move(arr_var));
	}

	return arr_var;
}

std::unique_ptr<Expression> Parser::ParseArrayLiteral()
{
	if (Match(Token::Type::LEFT_BRACKET))
	{
		Token& op = Previous();
		std::vector<std::unique_ptr<Expression>> expr_vector;

		if (Match(Token::Type::RIGHT_BRACKET))
		{
			return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::nullopt));
		}
		else if (Match(Token::Type::AT))
		{
			std::unique_ptr<Expression> allocated_size = ParseTernary();
			Consume(Token::Type::RIGHT_BRACKET, "Expected ']' for array expression.");
			return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::move(allocated_size)));
		}

		do
		{
			expr_vector.emplace_back(ParseArrayLiteral());
		} while (Match(Token::Type::COMMA));

		Consume(Token::Type::RIGHT_BRACKET, "Expected ']' for array expression.");

		return std::make_unique<Expression>(Array(std::move(op), std::move(expr_vector), std::nullopt));
	}

	return ParseTernary();
}

std::unique_ptr<Expression> Parser::ParseCall()
{
	std::unique_ptr<Expression> expr = ParsePrimary();

	while (true)
	{
		if (Match(Token::Type::LEFT_PAREN))
		{
			expr = FinishCall(std::move(expr));
		}
		else if (Match(Token::Type::DOT))
		{
			Token name = Consume(Token::Type::IDENTIFIER, "Expected identifier after '.'.");
			expr = std::make_unique<Expression>(Get(std::move(name), std::move(expr)));
		}
		else
		{
			break;
		}
	}

	return expr;
}

std::unique_ptr<Expression> Parser::FinishCall(std::unique_ptr<Expression>&& callee)
{
	std::vector<std::unique_ptr<Expression>> arguments;
	if (!Check(Token::Type::RIGHT_PAREN, 0))
	{
		do
		{
			arguments.emplace_back(ParseExpression());
		} while (Match(Token::Type::COMMA));
	}

	Token paren = Consume(Token::Type::RIGHT_PAREN, "Expected ')' after arguments.");

	return std::make_unique<Expression>(Call(std::move(paren), std::move(callee), std::move(arguments)));
}

std::unique_ptr<Expression> Parser::ParsePrimary()
{
	if (Match(Token::Type::LEFT_PAREN))
	{
		std::unique_ptr<Expression> expr_in = ParseExpression();
		Consume(Token::Type::RIGHT_PAREN, "Expected right parentheses.");
		return std::make_unique<Expression>(Group(std::move(expr_in)));
	}
	else if (Match(Token::Type::IDENTIFIER))
	{
		Token variable = Previous();

		for (std::vector<std::unordered_map<std::string, int>>::reverse_iterator scope_it = m_scopes.rbegin(); scope_it != m_scopes.rend(); ++scope_it) 
		{
			std::unordered_map<std::string, int>::const_iterator find_result = scope_it->find(variable.m_lexeme);
			if (find_result != scope_it->end()) 
			{
				// global
				if (scope_it == std::prev(m_scopes.rend()))
				{
					return std::make_unique<Expression>(Variable(std::move(Previous()), std::nullopt));
				}
				// local
				else
				{
					return std::make_unique<Expression>(Variable(std::move(Previous()), find_result->second));
				}
			}
		}

		CompilerError::PrintError(CompilerError::Type::PARSER, "Undefined Variable.", variable);
		m_error = true;
		Synchronize();
		return nullptr;
	}
	else if (Match(Token::Type::BACKSLASH))
	{
		Token& backslash = Previous();
		std::vector<Token> params;

		BeginScope();

		do
		{
			std::optional<Token> param_name = DefineName();
			if (param_name.has_value())
			{
				params.emplace_back(param_name.value());
				GetLocalVariableIndex(param_name.value().m_lexeme);
			}
			else
			{
				return nullptr;
			}
		} while (!Check(Token::Type::RIGHT_ARROW, 0));

		Consume(Token::Type::RIGHT_ARROW, "Expected '->' after bound variables.");

		Consume(Token::Type::LEFT_BRACE, "Expected '{' before lambda expression body.");

		std::unique_ptr<Statement> body = ParseBlockStatement();

		EndScope();

		return std::make_unique<Expression>(Lambda(std::move(backslash), std::move(params), std::move(body)));
	}
	else if (Match(Token::Type::TRUE, Token::Type::FALSE))
	{
		return std::make_unique<Expression>(Bool(std::move(Previous())));
	}
	else if (Match(Token::Type::NIL))
	{
		return std::make_unique<Expression>(Nil(std::move(Previous())));
	}
	else if (Match(Token::Type::NUMBER))
	{
		return std::make_unique<Expression>(Number(std::move(Previous())));
	}
	else if (Match(Token::Type::STRING))
	{
		return std::make_unique<Expression>(String(std::move(Previous())));
	}
	else
	{
		CompilerError::PrintError(CompilerError::Type::PARSER, "Expected expression.", Previous());
		m_error = true;
		Synchronize();
		return nullptr;
	}
}

std::unique_ptr<Expression> Parser::ParseLogicalAnd()
{
	std::unique_ptr<Expression> expr = ParsePipe();

	while (Match(Token::Type::DOUBLE_AMPERSAND))
	{
		Token& op = Previous();
		std::unique_ptr<Expression> right = ParsePipe();
		expr = std::make_unique<Expression>(Logical(std::move(op), std::move(expr), std::move(right)));
	}

	return expr;
}

std::unique_ptr<Expression> Parser::ParseLogicalOr()
{
	std::unique_ptr<Expression> expr = ParseLogicalAnd();

	while (Match(Token::Type::DOUBLE_BAR))
	{
		Token& op = Previous();
		std::unique_ptr<Expression> right = ParseLogicalAnd();
		expr = std::make_unique<Expression>(Logical(std::move(op), std::move(expr), std::move(right)));
	}

	return expr;
}

std::unique_ptr<Expression> Parser::ParsePipe()
{
	std::unique_ptr<Expression> expr = ParseBitwiseOr();

	while (Match(Token::Type::PIPE))
	{
		Token& op = Previous();
		std::unique_ptr<Expression> right = ParseBitwiseOr();
		expr = std::make_unique<Expression>(Pipe(std::move(op), std::move(expr), std::move(right)));
	}

	return expr;
}

std::unique_ptr<Statement> Parser::ParseDeclaration()
{
	if (Match(Token::Type::LET))
	{
		return ParseLetStatement();
	}
	else if (Match(Token::Type::NAMESPACE))
	{
		return ParseNamespaceDeclaration();
	}
	return ParseStatement();
}

std::unique_ptr<Statement> Parser::ParseBlockStatement()
{
	std::vector<std::unique_ptr<Statement>> statements;
	BeginScope();

	while (!IsAtEnd() && !Check(Token::Type::RIGHT_BRACE, 0))
	{
		statements.emplace_back(ParseDeclaration());
	}

	Consume(Token::Type::RIGHT_BRACE, "Expected '}' after block.");
	Token right_brace = Previous();
	int block_local_count = EndScope();

	return std::make_unique<Statement>(Block(std::move(right_brace), std::move(statements), std::move(block_local_count)));
}

std::unique_ptr<Statement> Parser::ParseLetStatement()
{
	std::optional<Token> name_opt = DefineName();
	if (!name_opt.has_value())
	{
		return nullptr;
	}

	Token name = name_opt.value();

	std::optional<int> local_index = GetLocalVariableIndex(name.m_lexeme);

	if (Match(Token::Type::SINGLE_EQUAL))
	{
		std::unique_ptr<Expression> initializer = ParseExpression();
		Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after variable declaration.");
		return std::make_unique<Statement>(Let(std::move(name), std::move(initializer), std::move(local_index)));
	}
	else
	{
		Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after variable declaration.");
		return std::make_unique<Statement>(Let(std::move(name), nullptr, std::move(local_index)));
	}
}

std::unique_ptr<Statement> Parser::ParseNamespaceDeclaration()
{
	Token name = Consume(Token::Type::IDENTIFIER, "Expected module name.");
	Consume(Token::Type::LEFT_BRACE, "Expected '{' before module body.");
	return std::make_unique<Statement>(Namespace(std::move(name), ParseBlockStatement()));
}

std::unique_ptr<Statement> Parser::ParseIfStatement()
{
	Token if_token = Previous();
	Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"if\".");
	std::unique_ptr<Expression> condition = ParseExpression();
	Consume(Token::Type::RIGHT_PAREN, "Expected ')' after if condition.");

	std::unique_ptr<Statement> then_branch = ParseStatement();
	std::optional<std::unique_ptr<Statement>> else_branch = std::nullopt;
	std::optional<Token> else_token = std::nullopt;
	if (Match(Token::Type::ELSE))
	{
		else_token.emplace(Previous());
		else_branch.emplace(ParseStatement());
	}

	return std::make_unique<Statement>(If(std::move(if_token), std::move(else_token), std::move(condition), std::move(then_branch), std::move(else_branch)));
}

std::unique_ptr<Statement> Parser::ParseWhileStatement()
{
	Token m_keyword = Previous();

	Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"while\".");
	std::unique_ptr<Expression> condition = ParseExpression();

	Consume(Token::Type::RIGHT_PAREN, "Expected ')' after condition.");
	std::unique_ptr<Statement> body = ParseStatement();

	return std::make_unique<Statement>(While(std::move(m_keyword), std::move(condition), std::move(body)));
}

std::unique_ptr<Statement> Parser::ParseForStatement()
{
	Token m_keyword = Previous();

	BeginScope();

	Consume(Token::Type::LEFT_PAREN, "Expected '(' after \"for\".");
	std::unique_ptr<Statement> initializer;
	if (Match(Token::Type::LET))
	{
		initializer = ParseLetStatement();
	}
	else if (!Match(Token::Type::SINGLE_SEMICOLON))
	{
		initializer = ParseSimpleStatement();
	}

	std::optional<std::unique_ptr<Expression>> condition = std::nullopt;
	if (!Check(Token::Type::SINGLE_SEMICOLON, 0))
	{
		condition.emplace(ParseExpression());
	}
	Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"for\" condition.");

	std::optional<std::unique_ptr<Statement>> increment = std::nullopt;
	if (!Check(Token::Type::RIGHT_PAREN, 0))
	{
		increment.emplace(std::make_unique<Statement>(Simple(ParseExpression())));
	}
	Consume(Token::Type::RIGHT_PAREN, "Expected ')' after \"for\" clauses.");

	std::unique_ptr<Statement> body = ParseStatement();

	int control_block_local_count = EndScope();

	body = std::make_unique<Statement>(For(std::move(m_keyword), std::move(initializer), std::move(condition), std::move(increment), std::move(body), std::move(control_block_local_count)));

	return body;
}

std::unique_ptr<Statement> Parser::ParseBreakStatement()
{
	Token& keyword = Previous();
	Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"break\".");
	return std::make_unique<Statement>(Break(std::move(keyword)));
}

std::unique_ptr<Statement> Parser::ParseContinueStatement()
{
	Token& keyword = Previous();
	Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"continue\".");
	return std::make_unique<Statement>(Continue(std::move(keyword)));
}

std::unique_ptr<Statement> Parser::ParseSimpleStatement()
{
	std::unique_ptr<Expression> expr = ParseExpression();
	Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after expression.");
	return std::make_unique<Statement>(Simple(std::move(expr)));
}

std::unique_ptr<Statement> Parser::ParseHaltStatement()
{
	Token& keyword = Previous();
	std::unique_ptr<Expression> message = ParseExpression();

	Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after \"halt\".");

	return std::make_unique<Statement>(Halt(std::move(keyword), std::move(message)));
}

std::unique_ptr<Statement> Parser::ParseImportStatement()
{
	Token& keyword = Previous();
	std::unique_ptr<Expression> path = ParseExpression();
	Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after script path.");
	return std::make_unique<Statement>(Import(std::move(keyword), std::move(path)));
}

std::unique_ptr<Statement> Parser::ParseReturnStatement()
{
	Token& keyword = Previous();
	std::optional<std::unique_ptr<Expression>> value = std::nullopt;
	if (!Check(Token::Type::SINGLE_SEMICOLON, 0))
	{
		value.emplace(ParseExpression());
	}

	Consume(Token::Type::SINGLE_SEMICOLON, "Expected ';' after return value.");

	return std::make_unique<Statement>(Return(std::move(keyword), std::move(value)));
}


std::unique_ptr<Statement> Parser::ParseStatement()
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
	else if (Match(Token::Type::HALT))
	{
		return ParseHaltStatement();
	}
	else
	{
		return ParseSimpleStatement();
	}
}

Parser::ParserResult Parser::Parse()
{
	ProgramTree program;
	while (!IsAtEnd())
	{
		program.emplace_back(ParseDeclaration());
	}

	return ParserResult(std::move(program), m_error);
}

void Parser::Synchronize()
{
	Advance();

	while (!IsAtEnd())
	{
		if (Previous().m_type == Token::Type::SINGLE_SEMICOLON)
		{
			return;
		}
		switch (Peek(0).m_type)
		{
		case Token::Type::NAMESPACE:
		case Token::Type::FUN:
		case Token::Type::LET:
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