#pragma once

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"
#include "Compiler/Token/Token.h"
#include "Common/Value/Value.h"

#include <format>

class MidoriError
{
private:
	static std::string GenerateBaseError(std::string&& message, int line, const Token* token = nullptr);

public:
	static std::string GenerateCodeGeneratorError(std::string_view message, int line);

	static std::string GenerateLexerError(std::string_view message, int line);

	static std::string GenerateParserError(std::string_view message, const Token& token);

	template <typename... ExpectedTypes>
	static std::string GenerateTypeCheckerError(std::string_view message, const Token& token, const MidoriType* actual, ExpectedTypes&&... expected)
	{
		if constexpr (sizeof...(expected) == 0)
		{
			return GenerateBaseError(std::format("Type Checker Error\n{}", message), token.m_line, &token);
		}
		else
		{
			std::string expected_types;
			std::array<std::string, sizeof...(expected)> expected_names{ MidoriTypeUtil::GetTypeName(expected)... };

			if constexpr (expected_names.size() > 1u)
			{
				for (size_t i = 0u; i < expected_names.size(); i += 1u)
				{
					expected_types.append(expected_names[i]);
					if (i != expected_names.size() - 1)
					{
						expected_types.append(" or ");
					}
				}
			}
			else
			{
				expected_types = expected_names[0];
			}

			return GenerateBaseError(std::format("Type Checker Error\n{}\nExpected {}, but got {}", message, expected_types, MidoriTypeUtil::GetTypeName(actual)), token.m_line, &token);
		}
	}

	static std::string GenerateRuntimeError(std::string_view message, int line);
};