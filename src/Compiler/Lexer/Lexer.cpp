#include "Lexer.h"

const std::unordered_map<std::string, Token::Type> Lexer::s_keywords = 
{
    // reserved keywords
    {"else", Token::Type::ELSE},
    {"false", Token::Type::FALSE},
    {"for", Token::Type::FOR},
    {"if", Token::Type::IF},
    {"return", Token::Type::RETURN},
    {"true", Token::Type::TRUE},
    {"let", Token::Type::LET},
    {"function", Token::Type::FUN},
    {"while", Token::Type::WHILE},
    {"do", Token::Type::DO},
    {"break", Token::Type::BREAK},
    {"continue", Token::Type::CONTINUE},
    {"import", Token::Type::IMPORT},
    {"namespace", Token::Type::NAMESPACE}, 
};

std::optional<std::string> Lexer::SkipWhitespaceAndComments()
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
                    return CompilerError::GenerateLexerError("Unterminated block comment.", m_line);
                }

                // The closing "*/".
                Advance();
                Advance();
            }
            else
            {
                return std::nullopt;
            }
            break;
        default:
            return std::nullopt;
        }
    }
}

Result::TokenResult Lexer::MatchString()
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
                result += '\f'; 
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
        return std::unexpected(CompilerError::GenerateLexerError("Unterminated string.", m_line));
    }
    else
    {
        Advance();
    }

    return MakeToken(Token::Type::STRING, std::move(result));  
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

Result::TokenResult Lexer::LexOneToken()
{
    if (SkipWhitespaceAndComments().has_value())
    {
        return std::unexpected(SkipWhitespaceAndComments().value());
    }

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
    case '#':
        return MakeToken(Token::Type::HASH);
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
        if (MatchNext('+'))
        {
            return MakeToken(Token::Type::DOUBLE_PLUS);
        }
        else
        {
            return MakeToken(Token::Type::SINGLE_PLUS);
        }
    case '-':
        return MakeToken(Token::Type::MINUS);
    case '?':
        return MakeToken(Token::Type::QUESTION);
    case ':':
        return MakeToken(Token::Type::SINGLE_COLON);
    case '%':
        return MakeToken(Token::Type::PERCENT);
    case '*':
        return MakeToken(Token::Type::STAR);
    case '/':
        return MakeToken(Token::Type::SLASH);
    case '|':
        if (MatchNext('|'))
        {
            return MakeToken(Token::Type::DOUBLE_BAR);
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
            return std::unexpected(CompilerError::GenerateLexerError("Invalid character.", m_line));
        }
    }
}

Result::LexerResult Lexer::Lex()
{
    TokenStream token_stream;
    std::vector<std::string> errors;

    while (!IsAtEnd(0))
    {
        std::expected<Token, std::string> result = LexOneToken();
        if (result.has_value())
        {
			token_stream.AddToken(std::move(result.value()));
		}
        else
        {
			errors.emplace_back(result.error());
		}
    }

    if (!errors.empty())
    {
        return std::unexpected(errors);
    }

    if (std::prev(token_stream.cend())->m_token_type != Token::Type::END_OF_FILE)
    {
        token_stream.AddToken(MakeToken(Token::Type::END_OF_FILE));
    }
    
    return token_stream;
}