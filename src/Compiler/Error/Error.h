#pragma once

#include "Compiler/Token/Token.h"
#include <iostream>
#include <sstream>

class CompilerError
{
public:
	enum class Type
	{
		LEXER,
		PARSER,
		CODE_GENERATOR,
	};

private:
	static inline void PrintBaseError(Type type, const char* message, int line, const Token* token = nullptr)
	{
		std::stringstream ss;

		if (token != nullptr)
		{
			ss << "[line " << line << "] '" << token->m_lexeme << "' ";
		}
		else
		{
			ss << "[line " << line << "] ";
		}

		switch (type)
		{
		case Type::LEXER: ss << "Lexer Error: "; break;
		case Type::PARSER: ss << "Parser Error: "; break;
		case Type::CODE_GENERATOR: ss << "Code Generator Error: "; break;
		}

		ss << message;
		std::cerr << ss.str() << std::endl;
	}

public:
	static inline void PrintError(const char* message, int line)
	{
		PrintBaseError(Type::CODE_GENERATOR, message, line);
	}

	static inline void PrintError(Type type, const char* message, int line)
	{
		PrintBaseError(type, message, line);
	}

	static inline void PrintError(Type type, const char* message, const Token& token)
	{
		PrintBaseError(type, message, token.m_line, &token);
	}
};