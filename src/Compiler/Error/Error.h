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
	static inline void PrintBaseError(Type type, const char* message, const Token* token = nullptr)
	{
		std::stringstream ss;

		if (token != nullptr)
		{
			ss << "[line " << token->m_line << "] '" << token->m_lexeme << "' ";
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
	static inline void PrintError(const char* message)
	{
		PrintBaseError(Type::CODE_GENERATOR, message);
	}

	static inline void PrintError(Type type, const char* message, const Token& token)
	{
		PrintBaseError(type, message, &token);
	}
};