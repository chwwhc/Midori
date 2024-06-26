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
		std::optional<int> m_relative_index = std::nullopt;
		std::optional<int> m_absolute_index = std::nullopt;
		std::optional<int> m_closure_depth = std::nullopt;
		bool m_is_fixed = true;

		VariableContext() = default;

		VariableContext(bool is_fixed) : m_is_fixed(is_fixed) {}

		VariableContext(int relative_index, int absolute_index, int closure_depth, bool is_fixed) : m_relative_index(relative_index), m_absolute_index(absolute_index), m_closure_depth(closure_depth), m_is_fixed(is_fixed) {}
	};

	struct Scope
	{
		using VariableTable = std::unordered_map<std::string, VariableContext>;
		using StructConstructorTable = std::unordered_map<std::string, const MidoriType*>;
		using UnionConstructorTable = std::unordered_map<std::string, const MidoriType*>;
		using DefinedTypeTable = std::unordered_map<std::string, const MidoriType*>;
		using DefinedNames = std::unordered_set<std::string>;

		VariableTable m_variables;
		StructConstructorTable m_struct_constructors;
		UnionConstructorTable m_union_constructors;
		DefinedTypeTable m_defined_types;
		DefinedNames m_defined_names;
	};

	using DependencyGraph = std::unordered_map<std::string, std::vector<std::string>>;
	using Scopes = std::vector<Scope>;
	
	inline static DependencyGraph s_dependency_graph{};
	TokenStream m_tokens;
	std::string m_file_name;
	Scopes m_scopes;
	std::stack<int> m_local_count_before_loop;
	std::vector<std::string> m_namespaces;
	int m_closure_depth = 0;
	int m_current_token_index = 0;
	int m_total_locals_in_curr_scope = 0;
	int m_total_variables = 0;

public:
	Parser(TokenStream&& tokens, const std::string& file_name);

	MidoriResult::ParserResult Parse();

private:

	template <typename... T>
		requires (std::is_same_v<T, Token::Name> && ...)
	bool Match(T... tokens)
	{
		if ((... || Check(tokens, 0)))
		{
			Advance();
			return true;
		}
		return false;
	}

	template <typename... T>
		requires (std::is_same_v<T, Token::Name> && ...)
	MidoriResult::ExpressionResult ParseBinary(MidoriResult::ExpressionResult(Parser::* operand)(), T... tokens)
	{
		MidoriResult::ExpressionResult lower_expr = (this->*operand)();
		if (!lower_expr.has_value())
		{
			return std::unexpected<std::string>(std::move(lower_expr.error()));
		}

		while (Match(tokens...))
		{
			const Token& op = Previous();
			MidoriResult::ExpressionResult right = (this->*operand)();
			if (!right.has_value())
			{
				return std::unexpected<std::string>(std::move(right.error()));
			}

			lower_expr = std::make_unique<MidoriExpression>(Binary(std::move(op), std::move(lower_expr.value()), std::move(right.value())));
		}

		return lower_expr;
	}

	bool IsGlobalName(const std::vector<Scope>::const_reverse_iterator& found_scope_it) const;

	bool IsLocalName(const Scope::VariableTable::const_iterator& found_scope_it) const;

	void Synchronize();

	std::string GenerateParserError(std::string&& message, const Token& token);

	bool IsAtEnd();

	bool Check(Token::Name type, int offset);

	bool IsAtGlobalScope() const;

	std::vector<Scope>::const_reverse_iterator FindVariableScope(std::string& name);

	std::vector<Scope>::const_reverse_iterator FindTypeScope(std::string& name);

	Token& Peek(int offset);

	Token& Previous();

	Token& Advance();

	MidoriResult::TokenResult Consume(Token::Name type, std::string_view message);

	MidoriResult::TokenResult MatchNameResolution();

	void BeginScope();

	int EndScope();

	std::string Mangle(std::string_view name);

	MidoriResult::TokenResult DefineName(Token& name, bool is_fixed, bool is_variable);

	std::optional<int> RegisterOrUpdateLocalVariable(const std::string& name, bool is_fixed);

	bool HasReturnStatement(const MidoriStatement& stmt);

	MidoriResult::TypeResult ParseType(bool is_foreign = false);

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

	MidoriResult::ExpressionResult ParseUnaryLogicalBitwise();

	MidoriResult::ExpressionResult ParseUnaryArithmetic();

	MidoriResult::ExpressionResult ParseArrayAccessHelper(std::unique_ptr<MidoriExpression>&& arr_var);

	MidoriResult::ExpressionResult ParseArrayAccess();

	MidoriResult::ExpressionResult ParseTernary();

	MidoriResult::ExpressionResult ParseCall();

	MidoriResult::ExpressionResult ParseAs();

	MidoriResult::ExpressionResult ParseConstruct();

	MidoriResult::ExpressionResult FinishCall(std::unique_ptr<MidoriExpression>&& callee);

	MidoriResult::ExpressionResult ParsePrimary();

	MidoriResult::ExpressionResult ParseLogicalAnd();

	MidoriResult::ExpressionResult ParseLogicalOr();

	MidoriResult::ExpressionResult ParseMatch();

	MidoriResult::StatementResult ParseDeclarationCommon(bool may_throw_error);

	MidoriResult::StatementResult ParseDeclaration();

	MidoriResult::StatementResult ParseGlobalDeclaration();

	MidoriResult::StatementResult ParseBlockStatement();

	MidoriResult::StatementResult ParseDefineStatement();

	MidoriResult::StatementResult ParseStructDeclaration();

	MidoriResult::StatementResult ParseUnionDeclaration();

	MidoriResult::StatementResult ParseIfStatement();

	MidoriResult::StatementResult ParseWhileStatement();

	MidoriResult::StatementResult ParseForStatement();

	MidoriResult::StatementResult ParseBreakStatement();

	MidoriResult::StatementResult ParseContinueStatement();

	MidoriResult::StatementResult ParseSimpleStatement();

	MidoriResult::StatementResult ParseReturnStatement();

	MidoriResult::StatementResult ParseForeignStatement();

	MidoriResult::StatementResult ParseSwitchStatement();

	MidoriResult::StatementResult ParseNamespaceStatement();

	MidoriResult::StatementResult ParseStatement();

	MidoriResult::TokenResult HandleDirective();

	bool HasCircularDependency() const; 
};
