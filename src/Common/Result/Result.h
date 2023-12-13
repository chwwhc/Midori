#pragma once

#include <vector>
#include <expected>
#include <optional>
#include <string>
#include <memory>

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"
#include "Common/Value/Value.h"
#include "Common/Executable/Executable.h"

namespace MidoriResult
{
	using TokenResult = std::expected<Token, std::string>;
	using LexerResult = std::expected<TokenStream, std::vector<std::string>>;
	using ExpressionResult = std::expected<std::unique_ptr<MidoriExpression>, std::string>;
	using StatementResult = std::expected<std::unique_ptr<MidoriStatement>, std::string>;
	using ParserResult = std::expected<MidoriProgramTree, std::vector<std::string>>;
	using TypeResult = std::expected<std::shared_ptr<MidoriType>, std::string>;
	using TypeCheckerResult = std::optional<std::vector<std::string>>;
	using CodeGeneratorResult = std::expected<MidoriExecutable, std::vector<std::string>>;
	using CompilerResult = std::expected<MidoriExecutable, std::vector<std::string>>;
}