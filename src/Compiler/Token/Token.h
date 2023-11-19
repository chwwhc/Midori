#pragma once

#include <vector>
#include <string>

struct Token
{
	enum class Name
	{
		// symbol
		THIN_ARROW,
		LEFT_PAREN,
		RIGHT_PAREN,
		LEFT_BRACE,
		RIGHT_BRACE,
		LEFT_BRACKET,
		RIGHT_BRACKET,
		COMMA,
		DOT,
		SINGLE_SEMICOLON,
		SINGLE_PLUS,
		DOUBLE_PLUS,
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
		COLON_EQUAL,
		SINGLE_EQUAL,
		DOUBLE_EQUAL,
		RIGHT_ANGLE,
		GREATER_EQUAL,
		LEFT_ANGLE,
		LESS_EQUAL,
		QUESTION,
		SINGLE_COLON,
		DOUBLE_COLON,
		AT,
		HASH,

		// Literal
		IDENTIFIER_LITERAL,
		TEXT_LITERAL,
		FRACTION_LITERAL,
		INTEGER_LITERAL,

		// reserved
		ELSE,
		FALSE,
		CLOSURE,
		FOR,
		IF,
		RETURN,
		TRUE,
		VAR,
		FIXED,
		WHILE,
		DO,
		BREAK,
		CONTINUE,
		IMPORT,
		NAMESPACE,
		AS,

		// types
		FRACTION,
		INTEGER,
		TEXT,
		BOOL,
		UNIT,
		ARRAY,

		END_OF_FILE,
	};

	Name m_token_type;
	std::string m_lexeme;
	int m_line = 0;
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