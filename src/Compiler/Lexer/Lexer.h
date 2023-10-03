#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Compiler/Error/Error.h"
#include "Compiler/Token/Token.h"

class Lexer
{
public:
    struct LexerResult
    {
		TokenStream m_tokens;
        bool m_error;

        LexerResult(TokenStream&& tokens, bool error) : m_tokens(std::move(tokens)), m_error(error) {}
	};

private:
    std::string m_source_code;
    int m_line = 1;
    int m_begin = 0;
    int m_current = 0;
    bool m_error = false;
    static const std::unordered_map<std::string, Token::Type> s_keywords;

public:

    explicit Lexer(std::string&& source_code) noexcept : m_source_code(std::move(source_code)) {}

    LexerResult Lex();

private:

    inline bool IsAtEnd(int offset) { return m_current + offset >= m_source_code.size(); }

    inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }

    inline bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'); }

    inline bool IsAlphaNumeric(char c) { return IsDigit(c) || IsAlpha(c) ;}

    inline char Advance() { return m_source_code[m_current++]; }

    inline char LookAhead(int offset) { return (IsAtEnd(0u) || m_current + offset >= m_source_code.size()) ? '\0' : m_source_code[m_current + offset]; }

    inline bool MatchNext(char expected) { return ((IsAtEnd(0u) || m_source_code[m_current] != expected) ? false : (++m_current, true)); }

    inline Token MakeToken(Token::Type type) {  return Token(type, m_source_code.substr(m_begin, m_current - m_begin), m_line);}

    Token LexOneToken();

    void SkipWhitespaceAndComments();

    Token MatchString();

    Token MatchNumber();

    Token MatchIdentifierOrReserved();
};