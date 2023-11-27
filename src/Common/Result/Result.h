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
#ifdef DEBUG
		std::vector<std::string> m_module_names;
#endif
		MidoriTraceable::GarbageCollectionRoots m_constant_roots;
		StaticData m_static_data;
		GlobalVariableTable m_global_table;

#ifdef DEBUG
		ExecutableModule(std::vector<BytecodeStream>&& bytecode_vector, std::vector<std::string>&& module_names, MidoriTraceable::GarbageCollectionRoots&& roots, StaticData&& static_data, GlobalVariableTable&& global_table) : m_modules(std::move(bytecode_vector)), m_module_names(std::move(module_names)), m_constant_roots(std::move(roots)), m_static_data(std::move(static_data)), m_global_table(std::move(global_table)) {}
#else
		ExecutableModule(std::vector<BytecodeStream>&& bytecode_vector, MidoriTraceable::GarbageCollectionRoots&& roots, StaticData&& static_data, GlobalVariableTable&& global_table) : m_modules(std::move(bytecode_vector)), m_constant_roots(std::move(roots)), m_static_data(std::move(static_data)), m_global_table(std::move(global_table)) {}
#endif
	};

	using TokenResult = std::expected<Token, std::string>;
	using LexerResult = std::expected<TokenStream, std::vector<std::string>>;
	using ExpressionResult = std::expected<std::unique_ptr<MidoriExpression>, std::string>;
	using StatementResult = std::expected<std::unique_ptr<MidoriStatement>, std::string>;
	using ParserResult = std::expected<MidoriProgramTree, std::vector<std::string>>;
	using TypeResult = std::expected<std::shared_ptr<MidoriType>, std::string>;
	using StaticAnalyzerResult = std::optional<std::vector<std::string>>;
	using CodeGeneratorResult = std::expected<ExecutableModule, std::vector<std::string>>;
	using CompilerResult = std::expected<ExecutableModule, std::vector<std::string>>;
}