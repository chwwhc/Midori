#pragma once

#include <vector>
#include <string>

struct Token
{
	enum class Type
	{
		// symbol
		LEFT_PAREN,
		RIGHT_PAREN,
		LEFT_BRACE,
		RIGHT_BRACE,
		LEFT_BRACKET,
		RIGHT_BRACKET,
		COMMA,
		DOT,
		SINGLE_SEMICOLON,
		PLUS,
		MINUS,
		LEFT_SHIFT,
		RIGHT_SHIFT,
		PERCENT,
		STAR,
		SLASH,
		SINGLE_BAR,
		DOUBLE_BAR,
		CARET,
		SINGLE_AMPERSAND,
		DOUBLE_AMPERSAND,
		BANG,
		BANG_EQUAL,
		SINGLE_EQUAL,
		DOUBLE_EQUAL,
		PIPE,
		GREATER,
		GREATER_EQUAL,
		LESS,
		LESS_EQUAL,
		QUESTION,
		SINGLE_COLON,
		DOUBLE_COLON,
		AT,

		// Literal
		IDENTIFIER,
		STRING,
		NUMBER,

		// reserved
		ELSE,
		FALSE,
		FUN,
		FOR,
		IF,
		NIL,
		RETURN,
		TRUE,
		LET,
		WHILE,
		DO,
		BREAK,
		CONTINUE,
		PRINT,
		IMPORT,
		NAMESPACE,
		HALT,

		END_OF_FILE,
		ERROR,
	};

	Type m_type;
	std::string m_lexeme;
	int m_line;

	Token(Type type, std::string&& lexeme, int line)
		: m_type(type), m_lexeme(std::move(lexeme)), m_line(line)
	{
	}
};

class TokenStream
{
public:

private:
	std::vector<Token> m_tokens;

public:

	// Adding iterator support
	using iterator = std::vector<Token>::iterator;
	using const_iterator = std::vector<Token>::const_iterator;

	iterator begin() { return m_tokens.begin(); }
	iterator end() { return m_tokens.end(); }

	const_iterator begin() const { return m_tokens.begin(); }
	const_iterator end() const { return m_tokens.end(); }

	const_iterator cbegin() const { return m_tokens.cbegin(); }
	const_iterator cend() const { return m_tokens.cend(); }

	inline void AddToken(Token&& token) { m_tokens.emplace_back(std::move(token)); }

	inline Token& operator[](int index) const { return const_cast<Token&>(m_tokens[index]); }

	inline int Size() const { return static_cast<int>(m_tokens.size()); }

private:
};