#pragma once

#include <deque>
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
		SINGLE_MINUS,
		DOUBLE_MINUS,
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
		RIGHT_ANGLE,
		GREATER_EQUAL,
		LEFT_ANGLE,
		LESS_EQUAL,
		QUESTION,
		SINGLE_COLON,
		DOUBLE_COLON,
		AT,
		HASH,
		TILDE,
		COLON_PLUS,
		PLUS_COLON,

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
		STRUCT,
		UNION,
		NEW,
		AS,
		FOREIGN,
		CASE,
		DEFAULT,
		SWITCH,
		NAMESPACE,

		// types
		FRACTION,
		INTEGER,
		TEXT,
		BOOL,
		UNIT,
		ARRAY,

		// directive
		DIRECTIVE,

		END_OF_FILE,
	};

	std::string m_lexeme;
	Name m_token_name;
	int m_line;

	Token(std::string&& lexeme, Name token_name, int line) noexcept;
};

class TokenStream
{
public:

private:
	std::deque<Token> m_tokens;

public:
	using iterator = std::deque<Token>::iterator;
	using const_iterator = std::deque<Token>::const_iterator;

	iterator begin();

	iterator end();

	const_iterator cbegin() const;

	const_iterator cend() const;

	void AddToken(Token&& token);

	Token& operator[](int index) const;

	int Size() const;

	void Insert(iterator iter, TokenStream&& tokens);

	void Erase(iterator iter);
};