#pragma once

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"
#include "Compiler/Token/Token.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/StaticData.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Common/Value/Value.h"

#include <expected>

namespace Result
{
	struct GeneratedBytecodeBundle
	{
		BytecodeStream m_module;
		Traceable::GarbageCollectionRoots m_constant_roots;
		StaticData m_static_data;
		GlobalVariableTable m_global_table;
#ifdef DEBUG
		std::vector<const Object::DefinedFunction*> m_sub_modules;
#endif

#ifdef DEBUG
		GeneratedBytecodeBundle(BytecodeStream&& bytecode, Traceable::GarbageCollectionRoots&& roots, std::vector<const Object::DefinedFunction*>&& modules, StaticData&& static_data, GlobalVariableTable&& global_table) : m_module(std::move(bytecode)), m_constant_roots(std::move(roots)), m_sub_modules(std::move(modules)), m_static_data(std::move(static_data)), m_global_table(std::move(global_table)) {}
#else
		GeneratedBytecodeBundle(BytecodeStream&& bytecode, Traceable::GarbageCollectionRoots&& roots, StaticData&& static_data, GlobalVariableTable&& global_table) : m_module(std::move(bytecode)), m_constant_roots(std::move(roots)), m_static_data(std::move(static_data)), m_global_table(std::move(global_table)) {}
#endif
	};

	struct ExecutableModule
	{
		BytecodeStream m_bytecode;
		Traceable::GarbageCollectionRoots m_constant_roots;
		StaticData m_static_data;
		GlobalVariableTable m_global_table;
	};

	using TokenResult = std::expected<Token, std::string>;
	using LexerResult = std::expected<TokenStream, std::vector<std::string>>;
	using ExpressionResult = std::expected<std::unique_ptr<Expression>, std::string>;
	using StatementResult = std::expected<std::unique_ptr<Statement>, std::string>;
	using ParserResult = std::expected<ProgramTree, std::vector<std::string>>;
	using CodeGeneratorResult = std::expected<GeneratedBytecodeBundle, std::vector<std::string>>;
	using CompilerResult = std::expected<ExecutableModule, std::vector<std::string>>;
}

class CompilerError
{
private:
	static inline std::string GenerateBaseError(const char* message, int line, const Token* token = nullptr)
	{
		std::string generated_message;

		if (token != nullptr)
		{
			generated_message.append("[line ");
			generated_message.append(std::to_string(line));
			generated_message.append("] '");
			generated_message.append(token->m_lexeme);
			generated_message.append("' ");
		}
		else
		{
			generated_message.append("[line ");
			generated_message.append(std::to_string(line));
			generated_message.append("] ");
		}

		generated_message.append(message);

		return generated_message;
	}

public:
	static inline std::string GenerateCodeGeneratorError(const char* message, int line)
	{
		std::string generated_message = "Code Generator Error: ";
		generated_message.append(GenerateBaseError(message, line).data());
		return generated_message;
	}

	static inline std::string GenerateLexerError(const char* message, int line)
	{
		std::string generated_message = "Lexer Error: ";
		generated_message.append(GenerateBaseError(message, line).data());
		return generated_message;
	}

	static inline std::string GenerateParserError(const char* message, const Token& token)
	{
		std::string generated_message = "Parser Error: ";
		generated_message.append(GenerateBaseError(message, token.m_line, &token).data());
		return generated_message;
	}
};