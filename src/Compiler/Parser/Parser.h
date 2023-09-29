#pragma once

#include "Compiler/Token/Token.h"
#include "Compiler/AbstractSyntaxTree/Statement.h"
#include "Compiler/Error/Error.h"

class Parser
{
public:

private:
	TokenStream m_tokens;
	uint32_t m_lambda_count = 0u;
	uint32_t m_current = 0u;

public:
	explicit Parser(TokenStream&& tokens) : m_tokens(std::move(tokens)) {}

	Program Parse();

private:

	inline bool IsAtEnd() { return Peek(0u).m_type == Token::Type::END_OF_FILE; }

	inline bool Check(Token::Type type, uint32_t offset) { return !IsAtEnd() && Peek(offset).m_type == type; }

	inline Token& Peek(uint32_t offset) { return m_current + offset < m_tokens.Size() ? m_tokens[m_current + offset] : m_tokens[m_tokens.Size() - 1u]; }

	inline Token& Previous() { return m_tokens[m_current - 1u]; }

	inline Token& Advance() { if (!IsAtEnd()) { m_current += 1u; } return Previous(); }

	inline Token& Consume(Token::Type type, const char* message) 
	{
		if (Check(type, 0u)) 
		{
			return Advance(); 
		}

		throw CompilerError(CompilerError::Type::PARSER, message, Peek(0u).m_line);
	}

	template <typename... T>
	inline bool Match(T... types)
	{
		if ((... || Check(types, 0)))
		{
			Advance();
			return true;
		}
		return false;
	}

	template <typename... T>
	inline std::unique_ptr<Expression> ParseBinary(std::unique_ptr<Expression>(Parser::* operand)(), T... types)
	{
		std::unique_ptr<Expression> lower_expr = (this->*operand)();

		while (Match(types...))
		{
			const Token& op = Previous();
			std::unique_ptr<Expression> right = (this->*operand)();
			lower_expr = std::make_unique<Expression>(Binary(std::move(op), std::move(lower_expr), std::move(right)));
		}

		return lower_expr;
	}

	std::unique_ptr<Expression> ParseExpression();

	std::unique_ptr<Expression> ParseFactor();

	std::unique_ptr<Expression> ParseShift();

	std::unique_ptr<Expression> ParseTerm();

	std::unique_ptr<Expression> ParseComparison();

	std::unique_ptr<Expression> ParseEquality();

	std::unique_ptr<Expression> ParseBitwiseAnd();

	std::unique_ptr<Expression> ParseBitwiseXor();

	std::unique_ptr<Expression> ParseBitwiseOr();

	std::unique_ptr<Expression> ParseAssignment();

	std::unique_ptr<Expression> ParseUnary();

	std::unique_ptr<Expression> ParseArrayAccessHelper(std::unique_ptr<Expression>&& arr_var);

	std::unique_ptr<Expression> ParseArrayAccess();

	std::unique_ptr<Expression> ParseArrayLiteral();

	std::unique_ptr<Expression> ParseTernary();

	std::unique_ptr<Expression> ParseCall();

	std::unique_ptr<Expression> FinishCall(std::unique_ptr<Expression>&& callee);

	std::unique_ptr<Expression> ParsePrimary();

	std::unique_ptr<Expression> ParseLogicalAnd();

	std::unique_ptr<Expression> ParseLogicalOr();

	std::unique_ptr<Expression> ParsePipe();

	std::unique_ptr<Expression> ParseComma();

	std::unique_ptr<Statement> ParseDeclaration();

	std::unique_ptr<Statement> ParseBlockStatement();

	std::unique_ptr<Statement> ParseVarDeclaration(bool is_fixed);

	std::unique_ptr<Statement> ParseNamespaceDeclaration(bool is_fixed);

	std::unique_ptr<Statement> ParseClassDeclaration(bool is_fixed);

	std::unique_ptr<Statement> ParseFunctionStatement(bool is_sig, bool is_fixed);

	std::unique_ptr<Statement> ParseIfStatement();

	std::unique_ptr<Statement> ParseWhileStatement();

	std::unique_ptr<Statement> ParseForStatement();

	std::unique_ptr<Statement> ParseBreakStatement();

	std::unique_ptr<Statement> ParseContinueStatement();

	std::unique_ptr<Statement> ParsePrintStatement();

	std::unique_ptr<Statement> ParseSimpleStatement();

	std::unique_ptr<Statement> ParseHaltStatement();

	std::unique_ptr<Statement> ParseImportStatement();

	std::unique_ptr<Statement> ParseReturnStatement();

	std::unique_ptr<Statement> ParseStatement();

	void Synchronize();
};
