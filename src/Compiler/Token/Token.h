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
        RIGHT_ARROW,
        AT,
        BACKSLASH,

        // Literal
        IDENTIFIER,
        STRING,
        NUMBER,

        // reserved
        CLASS,
        ELSE,
        FALSE,
        FUN,
        FOR,
        IF,
        NIL,
        RETURN,
        SUPER,
        THIS,
        TRUE,
        VAR,
        WHILE,
        DO,
        BREAK,
        CONTINUE,
        PRINT,
        AND,
        OR,
        SIG,
        IMPORT,
        NAMESPACE,
        FIXED,
        HALT,

        END_OF_FILE,
	};

	Type m_type;
	std::string m_lexeme;
	uint32_t m_line;
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

    inline uint32_t Size() const { return static_cast<uint32_t>(m_tokens.size()); }

private:
};