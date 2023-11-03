#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"

#include <unordered_map>
#include <expected>
#include <stack>

class Parser
{
public:

private:
	struct VariableContext
	{
		int m_relative_index = -1;
		int m_absolute_index = -1;
		int m_closure_depth = -1;
		bool m_is_fixed = true;

		VariableContext() = default;

		VariableContext(int relative_index, int absolute_index, int closure_depth, bool is_fixed) : m_relative_index(relative_index), m_absolute_index(absolute_index), m_closure_depth(closure_depth), m_is_fixed(is_fixed) {}
	};

	using Scope = std::unordered_map<std::string, VariableContext>;

	TokenStream m_tokens;
	std::vector<Scope> m_scopes;
	std::stack<int> m_local_count_before_loop;
	std::unordered_set<std::string> m_native_functions;
	int m_closure_depth = 0;
	int m_current_token_index = 0;
	int m_total_locals_in_curr_scope = 0;
	int m_total_variables = 0;
	bool m_error = false;

public:
	explicit Parser(TokenStream&& tokens) : m_tokens(std::move(tokens))
	{
		m_native_functions.emplace("PrintLine");
	}

	MidoriResult::ParserResult Parse();

private:

	void Synchronize();

	inline std::string GenerateParserError(const char* message, const Token& token)
	{
		Synchronize();
		return MidoriError::GenerateParserError(message, token);
	}

	inline bool IsAtEnd() { return Peek(0).m_token_type == Token::Name::END_OF_FILE; }

	inline bool Check(Token::Name type, int offset) { return !IsAtEnd() && Peek(offset).m_token_type == type; }

	inline Token& Peek(int offset) { return m_current_token_index + offset < m_tokens.Size() ? m_tokens[m_current_token_index + offset] : m_tokens[m_tokens.Size() - 1]; }

	inline Token& Previous() { return m_tokens[m_current_token_index - 1]; }

	inline Token& Advance() { if (!IsAtEnd()) { m_current_token_index += 1; } return Previous(); }

	inline MidoriResult::TokenResult Consume(Token::Name type, const char* message)
	{
		if (Check(type, 0))
		{
			return Advance();
		}

		return std::unexpected<std::string>(MidoriError::GenerateParserError(message, Previous()));
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
	inline MidoriResult::ExpressionResult ParseBinary(MidoriResult::ExpressionResult(Parser::* operand)(), T... types)
	{
		MidoriResult::ExpressionResult lower_expr = (this->*operand)();
		if (!lower_expr.has_value())
		{
			return std::unexpected<std::string>(std::move(lower_expr.error()));
		}

		while (Match(types...))
		{
			const Token& op = Previous();
			MidoriResult::ExpressionResult right = (this->*operand)();
			if (!right.has_value())
			{
				return std::unexpected<std::string>(std::move(right.error()));
			}

			lower_expr = std::make_unique<Expression>(Binary(std::move(op), std::move(lower_expr.value()), std::move(right.value())));
		}

		return lower_expr;
	}

	inline void BeginScope() 
	{ 
		m_scopes.emplace_back(Scope()); 
	}

	inline int EndScope()
	{
		int block_local_count = static_cast<int>(m_scopes.back().size());
		m_total_locals_in_curr_scope -= block_local_count;
		m_total_variables -= block_local_count;
		m_scopes.pop_back();
		return block_local_count;
	}

	inline MidoriResult::TokenResult DefineName(const Token& name, bool is_fixed)
	{
		Scope::const_iterator it = m_scopes.back().find(name.m_lexeme);
		if (it != m_scopes.back().end())
		{
			return std::unexpected<std::string>(MidoriError::GenerateParserError("Name already declared in this scope.", name));
		}
		else
		{
			constexpr int relative_index = -1;
			constexpr int absolute_index = -1;
			constexpr int closure_depth = -1;
			m_scopes.back()[name.m_lexeme] = VariableContext(relative_index, absolute_index, closure_depth, is_fixed);
		}

		return name;
	}

	inline std::optional<int> GetLocalVariableIndex(const std::string& name, bool is_fixed)
	{
		bool is_global = m_scopes.size() == 1u;
		std::optional<int> local_index = std::nullopt;

		if (!is_global)
		{
			m_scopes.back()[name] = VariableContext(m_total_locals_in_curr_scope++, m_total_variables++, m_closure_depth, is_fixed);
			local_index.emplace(m_scopes.back()[name].m_relative_index);
		}

		return local_index;
	}

	MidoriResult::ExpressionResult ParseExpression();

	MidoriResult::ExpressionResult ParseFactor();

	MidoriResult::ExpressionResult ParseShift();

	MidoriResult::ExpressionResult ParseTerm();

	MidoriResult::ExpressionResult ParseComparison();

	MidoriResult::ExpressionResult ParseEquality();

	MidoriResult::ExpressionResult ParseBitwiseAnd();

	MidoriResult::ExpressionResult ParseBitwiseXor();

	MidoriResult::ExpressionResult ParseBitwiseOr();

	MidoriResult::ExpressionResult ParseBind();

	MidoriResult::ExpressionResult ParseUnary();

	MidoriResult::ExpressionResult ParseArrayAccessHelper(std::unique_ptr<Expression>&& arr_var);

	MidoriResult::ExpressionResult ParseArrayAccess();

	MidoriResult::ExpressionResult ParseTernary();

	MidoriResult::ExpressionResult ParseCall();

	MidoriResult::ExpressionResult FinishCall(std::unique_ptr<Expression>&& callee);

	MidoriResult::ExpressionResult ParsePrimary();

	MidoriResult::ExpressionResult ParseLogicalAnd();

	MidoriResult::ExpressionResult ParseLogicalOr();

	MidoriResult::StatementResult ParseDeclaration();

	MidoriResult::StatementResult ParseDeclarationHelper();

	MidoriResult::StatementResult ParseBlockStatement();

	MidoriResult::StatementResult ParseDefineStatement();

	MidoriResult::StatementResult ParseNamespaceDeclaration();

	MidoriResult::StatementResult ParseIfStatement();

	MidoriResult::StatementResult ParseWhileStatement();

	MidoriResult::StatementResult ParseForStatement();

	MidoriResult::StatementResult ParseBreakStatement();

	MidoriResult::StatementResult ParseContinueStatement();

	MidoriResult::StatementResult ParseSimpleStatement();

	MidoriResult::StatementResult ParseImportStatement();

	MidoriResult::StatementResult ParseReturnStatement();

	MidoriResult::StatementResult ParseStatement();
};
