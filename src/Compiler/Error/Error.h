#pragma once

#include "Compiler/Token/Token.h"

#include <iostream>
#include <string>

namespace CompilerError
{
	enum class Type
	{
		LEXER,
		PARSER,
	};

	inline void PrintError(Type type, const char* message, const Token& token)
	{
		std::cerr << "[line " << token.m_line << "] " << "'" << token.m_lexeme << "' ";
		switch (type)
		{
			case CompilerError::Type::LEXER:
			{
				std::cerr << "Lexer Error: ";
				break;
			}
			case CompilerError::Type::PARSER:
			{
				std::cerr << "Parser Error: ";
				break;
			}
			default:
			{
				break; // unreachable
			}
		}
		std::cerr << message << std::endl;
	}
}