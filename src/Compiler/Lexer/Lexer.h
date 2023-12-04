#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"

#include <string>
#include <vector>
#include <unordered_map>

class Lexer
{
public:

private:
    std::string m_source_code;
    int m_line = 1;
    int m_begin = 0;
    int m_current = 0;
    static const std::unordered_map<std::string, Token::Name> s_keywords;

public:

    explicit Lexer(std::string&& source_code) noexcept : m_source_code(std::move(source_code)) {}

    MidoriResult::LexerResult Lex();

private:

    inline bool IsAtEnd(int offset) { return m_current + offset >= static_cast<int>(m_source_code.size()); }

    inline bool IsDigit(char c) { return isdigit(c); }

    inline bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'); }

    inline bool IsAlphaNumeric(char c) { return IsDigit(c) || IsAlpha(c) ;}

    inline char Advance() { return m_source_code[m_current++]; }

    inline char LookAhead(int offset) { return (IsAtEnd(0u) || m_current + offset >= static_cast<int>(m_source_code.size())) ? '\0' : m_source_code[m_current + offset]; }

    inline bool MatchNext(char expected) { return ((IsAtEnd(0u) || m_source_code[m_current] != expected) ? false : (++m_current, true)); }

    inline Token MakeToken(Token::Name type) {  return Token(type, m_source_code.substr(m_begin, m_current - m_begin), m_line);}

    inline Token MakeToken(Token::Name type, std::string&& lexeme) { return Token(type, std::move(lexeme), m_line); }

    MidoriResult::TokenResult LexOneToken();

    std::optional<std::string> SkipWhitespaceAndComments();

    MidoriResult::TokenResult MatchString();

    Token MatchNumber();

    Token MatchIdentifierOrReserved();
};