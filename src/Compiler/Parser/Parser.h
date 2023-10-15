#pragma once

#include "Compiler/Error/Error.h"

#include <unordered_map>
#include <expected>

class Parser
{
public:

private:
	struct VariableContext
	{
		int m_relative_index = 0;
		int m_absolute_index = 0;
	};

	using Scope = std::unordered_map<std::string, VariableContext>;

	TokenStream m_tokens;
	std::vector<Scope> m_scopes = { Scope() };
	int m_function_depth = 0;
	int m_current = 0;
	int m_total_locals = 0;
	int m_total_variables = 0;
	bool m_error = false;

public:
	explicit Parser(TokenStream&& tokens) : m_tokens(std::move(tokens))
	{
		m_scopes.back()["Print"] = { 0, 0 };
	}

	Result::ParserResult Parse();

private:

	void Synchronize();

	inline std::string GenerateParserError(const char* message, const Token& token)
	{
		Synchronize();
		return CompilerError::GenerateParserError(message, token);
	}

	inline bool IsAtEnd() { return Peek(0).m_token_type == Token::Type::END_OF_FILE; }

	inline bool Check(Token::Type type, int offset) { return !IsAtEnd() && Peek(offset).m_token_type == type; }

	inline Token& Peek(int offset) { return m_current + offset < m_tokens.Size() ? m_tokens[m_current + offset] : m_tokens[m_tokens.Size() - 1]; }

	inline Token& Previous() { return m_tokens[m_current - 1]; }

	inline Token& Advance() { if (!IsAtEnd()) { m_current += 1; } return Previous(); }

	inline Result::TokenResult Consume(Token::Type type, const char* message)
	{
		if (Check(type, 0))
		{
			return Advance();
		}

		return std::unexpected(CompilerError::GenerateParserError(message, Peek(0)));
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
	inline Result::ExpressionResult ParseBinary(Result::ExpressionResult(Parser::* operand)(), T... types)
	{
		Result::ExpressionResult lower_expr = (this->*operand)();
		if (!lower_expr.has_value())
		{
			return std::unexpected(std::move(lower_expr.error()));
		}

		while (Match(types...))
		{
			const Token& op = Previous();
			Result::ExpressionResult right = (this->*operand)();
			if (!right.has_value())
			{
				return std::unexpected(std::move(right.error()));
			}

			lower_expr = std::make_unique<Expression>(Binary(std::move(op), std::move(lower_expr.value()), std::move(right.value())));
		}

		return lower_expr;
	}

	inline void BeginScope() { m_scopes.emplace_back(Scope()); }

	inline int EndScope()
	{
		int block_local_count = static_cast<int>(m_scopes.back().size());
		m_total_locals -= block_local_count;
		m_total_variables -= block_local_count;
		m_scopes.pop_back();
		return block_local_count;
	}

	inline Result::TokenResult DefineName(const Token& name)
	{
		Scope::const_iterator it = m_scopes.back().find(name.m_lexeme);
		if (it != m_scopes.back().end())
		{
			return std::unexpected(CompilerError::GenerateParserError("Name already declared in this scope.", name));
		}
		else
		{
			m_scopes.back()[name.m_lexeme] = { -1, -1 };
		}

		return name;
	}

	inline std::optional<int> GetLocalVariableIndex(const std::string& name)
	{
		bool is_global = static_cast<int>(m_scopes.size()) == 1;
		std::optional<int> local_index = std::nullopt;
		if (!is_global)
		{
			m_scopes.back()[name] = { m_total_locals++, m_total_variables++ };
			local_index.emplace(m_scopes.back()[name].m_relative_index);
		}

		return local_index;
	}

	Result::ExpressionResult ParseExpression();

	Result::ExpressionResult ParseFactor();

	Result::ExpressionResult ParseShift();

	Result::ExpressionResult ParseTerm();

	Result::ExpressionResult ParseComparison();

	Result::ExpressionResult ParseEquality();

	Result::ExpressionResult ParseBitwiseAnd();

	Result::ExpressionResult ParseBitwiseXor();

	Result::ExpressionResult ParseBitwiseOr();

	Result::ExpressionResult ParseAssignment();

	Result::ExpressionResult ParseUnary();

	Result::ExpressionResult ParseArrayAccessHelper(std::unique_ptr<Expression>&& arr_var);

	Result::ExpressionResult ParseArrayAccess();

	Result::ExpressionResult ParseTernary();

	Result::ExpressionResult ParseCall();

	Result::ExpressionResult FinishCall(std::unique_ptr<Expression>&& callee);

	Result::ExpressionResult ParsePrimary();

	Result::ExpressionResult ParseLogicalAnd();

	Result::ExpressionResult ParseLogicalOr();

	Result::StatementResult ParseDeclaration();

	Result::StatementResult ParseBlockStatement();

	Result::StatementResult ParseLetStatement();

	Result::StatementResult ParseNamespaceDeclaration();

	Result::StatementResult ParseIfStatement();

	Result::StatementResult ParseWhileStatement();

	Result::StatementResult ParseForStatement();

	Result::StatementResult ParseBreakStatement();

	Result::StatementResult ParseContinueStatement();

	Result::StatementResult ParseSimpleStatement();

	Result::StatementResult ParseImportStatement();

	Result::StatementResult ParseReturnStatement();

	Result::StatementResult ParseStatement();
};
