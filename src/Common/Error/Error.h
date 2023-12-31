#pragma once

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"
#include "Compiler/Token/Token.h"
#include "Common/Value/Value.h"

#include <span>

class MidoriError
{
private:
	static std::string GenerateBaseError(std::string&& message, int line, const Token* token = nullptr);

public:
	static std::string GenerateCodeGeneratorError(std::string_view message, int line);

	static std::string GenerateLexerError(std::string_view message, int line);

	static std::string GenerateParserError(std::string_view message, const Token& token);

	static std::string GenerateTypeCheckerError(std::string_view message, const Token& token, const std::span<const MidoriType*>& expected, const MidoriType* actual);

	static std::string GenerateRuntimeError(std::string_view message, int line);
};