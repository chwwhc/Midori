#pragma once

#include <vector>
#include <expected>
#include <optional>
#include <string>
#include <memory>

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/Value.h"
#include "Common/Value/StaticData.h"
#include "Common/Value/GlobalVariableTable.h"

namespace MidoriResult
{
	struct ExecutableModule
	{
		std::vector<BytecodeStream> m_modules;
		Traceable::GarbageCollectionRoots m_constant_roots;
		StaticData m_static_data;
		GlobalVariableTable m_global_table;

		ExecutableModule(std::vector<BytecodeStream>&& bytecode_vector, Traceable::GarbageCollectionRoots&& roots, StaticData&& static_data, GlobalVariableTable&& global_table) : m_modules(std::move(bytecode_vector)), m_constant_roots(std::move(roots)), m_static_data(std::move(static_data)), m_global_table(std::move(global_table)) {}
	};

	using TokenResult = std::expected<Token, std::string>;
	using LexerResult = std::expected<TokenStream, std::vector<std::string>>;
	using ExpressionResult = std::expected<std::unique_ptr<Expression>, std::string>;
	using StatementResult = std::expected<std::unique_ptr<Statement>, std::string>;
	using ParserResult = std::expected<ProgramTree, std::vector<std::string>>;
	using StaticAnalyzerResult = std::optional<std::vector<std::string>>;
	using CodeGeneratorResult = std::expected<ExecutableModule, std::vector<std::string>>;
	using CompilerResult = std::expected<ExecutableModule, std::vector<std::string>>;
	using InterpreterResult = std::expected<MidoriValue*, std::string>;
}