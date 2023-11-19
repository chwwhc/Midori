#pragma once

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"
#include "Compiler/Token/Token.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/StaticData.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Common/Value/Value.h"

#include <format>

class MidoriError
{
private:
	static inline std::string GenerateBaseError(std::string&& message, int line, const Token* token = nullptr)
	{
		std::string generated_message;

		if (token != nullptr)
		{
			generated_message.append(std::format("[line {}] '{}' {}", line, token->m_lexeme, message));
		}
		else
		{
			generated_message.append(std::format("[line {}] {}", line, message));
		}

		return generated_message;
	}

public:
	static inline std::string GenerateCodeGeneratorError(std::string_view message, int line)
	{
		return GenerateBaseError(std::format("Code Generator Error\n{}", message), line);
	}

	static inline std::string GenerateLexerError(std::string_view message, int line)
	{
		return GenerateBaseError(std::format("Lexer Error\n{}", message), line);
	}

	static inline std::string GenerateParserError(std::string_view message, const Token& token)
	{
		return GenerateBaseError(std::format("Parser Error\n{}", message), token.m_line, &token);
	}

	static inline std::string GenerateTypeCheckerError(std::string_view message, const Token& token, const std::vector<const MidoriType*>& expected, const MidoriType& actual)
	{
		if (expected.empty())
		{
			return GenerateBaseError(std::format("Type Checker Error\n{}, got {}", message, MidoriTypeUtil::ToString(actual)), token.m_line, &token);
		}

		std::string expected_types;
		if (expected.size() > 1u)
		{
			for (size_t i = 0u; i < expected.size(); i += 1u)
			{
				expected_types.append(MidoriTypeUtil::ToString(*expected[i]));
				if (i != expected.size() - 1)
				{
					expected_types.append(", ");
				}
			}
		}
		else
		{
			expected_types = MidoriTypeUtil::ToString(*expected[0]);
		}

		return GenerateBaseError(std::format("Type Checker Error\n{}\nExpected {}, but got {}", message, expected_types, MidoriTypeUtil::ToString(actual)), token.m_line, &token);
	}

	static inline std::string GenerateRuntimeError(std::string_view message, int line)
	{
		return GenerateBaseError(std::format("Runtime Error\n{}", message), line);
	}
};