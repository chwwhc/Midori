#pragma once

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"
#include "Compiler/Token/Token.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/StaticData.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Common/Value/Value.h"

#include <expected>

class MidoriError
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

	static inline std::string GenerateStaticAnalyzerError(const char* message, const Token& token)
	{
		std::string generated_message = "Static Analyzer Error: ";
		generated_message.append(GenerateBaseError(message, token.m_line, &token).data());
		return generated_message;
	}

	static inline std::string GenerateRuntimeError(const char* message, int line)
	{
		std::string generated_message = "Runtime Error: ";
		generated_message.append(GenerateBaseError(message, line).data());
		return generated_message;
	}
};