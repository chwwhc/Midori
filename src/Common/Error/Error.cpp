#include "Error.h"

#include <format>

std::string MidoriError::GenerateBaseError(std::string&& message, int line, const Token* token)
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

std::string MidoriError::GenerateCodeGeneratorError(std::string_view message, int line)
{
	return GenerateBaseError(std::format("Code Generator Error\n{}", message), line);
}

std::string MidoriError::GenerateLexerError(std::string_view message, int line)
{
	return GenerateBaseError(std::format("Lexer Error\n{}", message), line);
}

std::string MidoriError::GenerateParserError(std::string_view message, const Token& token)
{
	return GenerateBaseError(std::format("Parser Error\n{}", message), token.m_line, &token);
}

std::string MidoriError::GenerateRuntimeError(std::string_view message, int line)
{
	return GenerateBaseError(std::format("Runtime Error\n{}", message), line);
}
