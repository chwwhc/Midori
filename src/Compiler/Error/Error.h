#pragma once

#include <exception>

class CompilerError : public std::exception
{
public:
	enum class Type
	{
		LEXER,
		PARSER,
	};

private:
	std::string m_message;

public:
	CompilerError(CompilerError::Type type, const char* message, uint32_t line)
	{
		switch (type)
		{
			case CompilerError::Type::LEXER:
			{
				m_message.append("Lexer Error: ");
				break;
			}
			case CompilerError::Type::PARSER:
			{
				m_message.append("Parser Error: ");
				break;
			}
			default:
			{
				break; // unreachable
			}
		}

		m_message.append(message);
		m_message.append("\n[line ");
		m_message.append(std::to_string(line));
		m_message.append("] in script\n");
	}
	const char* what() const noexcept override
	{
		return m_message.data();
	}

private:
};
