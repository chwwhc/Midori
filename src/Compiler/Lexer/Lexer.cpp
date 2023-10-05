#include "Lexer.h"

const std::unordered_map<std::string, Token::Type> Lexer::s_keywords = {
    {"else", Token::Type::ELSE},
    {"false", Token::Type::FALSE},
    {"for", Token::Type::FOR},
    {"if", Token::Type::IF},
    {"nil", Token::Type::NIL},
    {"return", Token::Type::RETURN},
    {"true", Token::Type::TRUE},
    {"let", Token::Type::LET},
    {"while", Token::Type::WHILE},
    {"do", Token::Type::DO},
    {"break", Token::Type::BREAK},
    {"continue", Token::Type::CONTINUE},
    {"import", Token::Type::IMPORT},
    {"namespace", Token::Type::NAMESPACE},
    {"halt", Token::Type::HALT}, };

void Lexer::SkipWhitespaceAndComments()
{
    while (true)
    {
        char c = LookAhead(0);

        switch (c)
        {
        case ' ':
        case '\r':
        case '\t':
            Advance();
            break;
        case '\n':
            m_line++;
            Advance();
            break;
        case '/':
            if (LookAhead(1) == '/')
            {
                // A comment goes until the end of the line.
                while (LookAhead(0) != '\n' && !IsAtEnd(0))
                {
                    Advance();
                }
            }
            else if (LookAhead(1) == '*')
            {
                // A block comment goes until the closing "*/".
                Advance();
                Advance();

                while (LookAhead(0) != '*' && LookAhead(1) != '/' && !IsAtEnd(0))
                {
                    if (LookAhead(0) == '\n')
                    {
                        m_line += 1;
                    }
                    Advance();
                }

                if (IsAtEnd(0))
                {
                    CompilerError::PrintError(CompilerError::Type::LEXER, "Unterminated block comment.", m_line);
                    m_error = true;
                    return;
                }

                // The closing "*/".
                Advance();
                Advance();
            }
            else
            {
                return;
            }
            break;
        default:
            return;
        }
    }
}

Token Lexer::MatchString()
{
    std::string result;

    while (LookAhead(0) != '"' && !IsAtEnd(0))
    {
        if (LookAhead(0) == '\n')
        {
            m_line += 1;
        }
        if (LookAhead(0) == '\\' && !IsAtEnd(1))
        {
            switch (LookAhead(1))
            {
            case 't':
                result += '\t';
                break;
            case 'n':
                result += '\n';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += 'f';
                break;
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            default:
                break;
            }

            Advance();
            Advance();
            continue;
        }

        result += Advance();
    }

    if (IsAtEnd(0))
    {
        CompilerError::PrintError(CompilerError::Type::LEXER, "Unterminated string.", m_line);
        m_error = true;
    }

    Advance();

    return MakeToken(Token::Type::STRING);
}

Token Lexer::MatchNumber()
{
    while (IsDigit(LookAhead(0)))
    {
        Advance();
    }

    // Look for a fractional part.
    if (LookAhead(0) == '.' && IsDigit(LookAhead(1)))
    {
        // Consume the ".".
        Advance();

        while (IsDigit(LookAhead(0)))
        {
            Advance();
        }
    }

    return MakeToken(Token::Type::NUMBER);
}

Token Lexer::MatchIdentifierOrReserved()
{
    while (IsAlphaNumeric(LookAhead(0)))
    {
        Advance();
    }

    std::string identifier = m_source_code.substr(m_begin, static_cast<size_t>(m_current - m_begin));

    std::unordered_map<std::string, Token::Type>::const_iterator it = s_keywords.find(identifier);

    if (it != s_keywords.cend())
    {
        return MakeToken(it->second);
    }
    else
    {
        return MakeToken(Token::Type::IDENTIFIER);
    }
}

Token Lexer::LexOneToken()
{
    SkipWhitespaceAndComments();
    if (IsAtEnd(0))
    {
		return MakeToken(Token::Type::END_OF_FILE);
	}

    m_begin = m_current;

    char next_char = Advance();
    switch (next_char)
    {
    case '(':
        return MakeToken(Token::Type::LEFT_PAREN);
    case ')':
        return MakeToken(Token::Type::RIGHT_PAREN);
    case '{':
        return MakeToken(Token::Type::LEFT_BRACE);
    case '}':
        return MakeToken(Token::Type::RIGHT_BRACE);
    case '[':
        return MakeToken(Token::Type::LEFT_BRACKET);
    case '@':
        return MakeToken(Token::Type::AT);
    case ']':
        return MakeToken(Token::Type::RIGHT_BRACKET);
    case ',':
        return MakeToken(Token::Type::COMMA);
    case '.':
        if (IsDigit(LookAhead(0)))
        {
            return MatchNumber();
        }
        else
        {
            return MakeToken(Token::Type::DOT);
        }
    case ';':
        return MakeToken(Token::Type::SINGLE_SEMICOLON);
    case '+':
        return MakeToken(Token::Type::PLUS);
    case '-':
        if (MatchNext('>'))
        {
            return MakeToken(Token::Type::RIGHT_ARROW);
        }
        else
        {
            return MakeToken(Token::Type::MINUS);
        }
    case '?':
        return MakeToken(Token::Type::QUESTION);
    case ':':
        return MakeToken(Token::Type::SINGLE_COLON);
    case '%':
        return MakeToken(Token::Type::PERCENT);
    case '*':
        return MakeToken(Token::Type::STAR);
    case '\\':
        return MakeToken(Token::Type::BACKSLASH);
    case '/':
        if (MatchNext('/'))
        {
            while (!IsAtEnd(0) && !MatchNext('\n'))
            {
                Advance();
            }
            m_line += 1;
        }
        else
        {
            return MakeToken(Token::Type::SLASH);
        }
    case '|':
        if (MatchNext('|'))
        {
            return MakeToken(Token::Type::DOUBLE_BAR);
        }
        else if (MatchNext('>'))
        {
            return MakeToken(Token::Type::PIPE);
        }
        else
        {
           return MakeToken(Token::Type::SINGLE_BAR);
        }
    case '^':
        return MakeToken(Token::Type::CARET);
    case '&':
        if (MatchNext('&'))
        {
			return MakeToken(Token::Type::DOUBLE_AMPERSAND);
		}
        else
        {
            return MakeToken(Token::Type::SINGLE_AMPERSAND);
        }
    case '!':
        if (MatchNext('='))
        {
            return MakeToken(Token::Type::BANG_EQUAL);
        }
        else
        {
            return MakeToken(Token::Type::BANG);
        }
    case '=':
        if (MatchNext('='))
        {
            return MakeToken(Token::Type::DOUBLE_EQUAL);
        }
        else
        {
            return MakeToken(Token::Type::SINGLE_EQUAL);
        }
    case '>':
        if (MatchNext('='))
        {
            return MakeToken(Token::Type::GREATER_EQUAL);
        }
        else if (MatchNext('>'))
        {
            return MakeToken(Token::Type::RIGHT_SHIFT);
        }
        else
        {
            return MakeToken(Token::Type::GREATER);
        }
    case '<':
        if (MatchNext('='))
        {
            return MakeToken(Token::Type::LESS_EQUAL);
        }
        else if (MatchNext('<'))
        {
            return MakeToken(Token::Type::LEFT_SHIFT);
        }
        else
        {
            return MakeToken(Token::Type::LESS);
        }
    case ' ':
    case '\r':
    case '\t':
        Advance();
        return LexOneToken();
    case '"':
        return MatchString();
    case '\n':
        m_line += 1;
        return LexOneToken();
    default:
        if (IsDigit(next_char))
        {
            return MatchNumber();
        }
        else if (IsAlpha(next_char))
        {
            return MatchIdentifierOrReserved();
        }
        else
        {
            CompilerError::PrintError(CompilerError::Type::LEXER, "Invalid character.", m_line);
            m_error = true;
            return Token(Token::Type::ERROR, "", m_line);
        }
    }
}

Lexer::LexerResult Lexer::Lex()
{
    TokenStream result;

    while (!IsAtEnd(0))
    {
        result.AddToken(LexOneToken());
    }

    if (std::prev(result.cend())->m_type != Token::Type::END_OF_FILE)
    {
        result.AddToken(MakeToken(Token::Type::END_OF_FILE));
    }
    
    return LexerResult(std::move(result), m_error);
}