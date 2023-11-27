#include "Lexer.h"

const std::unordered_map<std::string, Token::Name> Lexer::s_keywords =
{
	// types
	{"Fraction", Token::Name::FRACTION},
	{"Integer", Token::Name::INTEGER},
	{"Text", Token::Name::TEXT},
	{"Bool", Token::Name::BOOL},
	{"Unit", Token::Name::UNIT},
	{"Array", Token::Name::ARRAY},
	{"Maybe", Token::Name::MAYBE},

	// reserved keywords
	{"else", Token::Name::ELSE},
	{"false", Token::Name::FALSE},
	{"for", Token::Name::FOR},
	{"if", Token::Name::IF},
	{"return", Token::Name::RETURN},
	{"true", Token::Name::TRUE},
	{"var", Token::Name::VAR},
	{"fixed", Token::Name::FIXED},
	{"closure", Token::Name::CLOSURE},
	{"while", Token::Name::WHILE},
	{"do", Token::Name::DO},
	{"as", Token::Name::AS},
	{"break", Token::Name::BREAK},
	{"continue", Token::Name::CONTINUE},
	{"import", Token::Name::IMPORT},
	{"struct", Token::Name::STRUCT},
	{"union", Token::Name::UNION},
	{"new", Token::Name::NEW},
	{"Just", Token::Name::JUST},
	{"None", Token::Name::NONE},
};

std::optional<std::string> Lexer::SkipWhitespaceAndComments()
{
	while (!IsAtEnd(0))
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
				break;
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
					return MidoriError::GenerateLexerError("Unterminated block comment.", m_line);
				}

				// The closing "*/".
				Advance();
				Advance();

				break;
			}
			return std::nullopt;
		default:
			return std::nullopt;
		}
	}

	return std::nullopt;
}

MidoriResult::TokenResult Lexer::MatchString()
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
		return std::unexpected<std::string>(MidoriError::GenerateLexerError("Unterminated string.", m_line));
	}
	else
	{
		Advance();
	}

	return MakeToken(Token::Name::TEXT_LITERAL, std::move(result));
}

Token Lexer::MatchNumber()
{
	bool is_fraction = false;

	while (IsDigit(LookAhead(0)))
	{
		Advance();
	}

	// Look for a fractional part.
	if (LookAhead(0) == '.' && IsDigit(LookAhead(1)))
	{
		is_fraction = true;

		Advance();

		while (IsDigit(LookAhead(0)))
		{
			Advance();
		}
	}

	return MakeToken(is_fraction ? Token::Name::FRACTION_LITERAL : Token::Name::INTEGER_LITERAL);
}

Token Lexer::MatchIdentifierOrReserved()
{
	while (IsAlphaNumeric(LookAhead(0)))
	{
		Advance();
	}

	std::string identifier = m_source_code.substr(m_begin, static_cast<size_t>(m_current - m_begin));

	std::unordered_map<std::string, Token::Name>::const_iterator it = s_keywords.find(identifier);

	if (it != s_keywords.cend())
	{
		return MakeToken(it->second);
	}
	else
	{
		return MakeToken(Token::Name::IDENTIFIER_LITERAL);
	}
}

MidoriResult::TokenResult Lexer::LexOneToken()
{
	std::optional<std::string> error = SkipWhitespaceAndComments();
	if (error.has_value())
	{
		return std::unexpected<std::string>(std::move(error.value()));
	}

	if (IsAtEnd(0))
	{
		return MakeToken(Token::Name::END_OF_FILE);
	}

	m_begin = m_current;

	char next_char = Advance();
	switch (next_char)
	{
	case '(':
		return MakeToken(Token::Name::LEFT_PAREN);
	case ')':
		return MakeToken(Token::Name::RIGHT_PAREN);
	case '{':
		return MakeToken(Token::Name::LEFT_BRACE);
	case '}':
		return MakeToken(Token::Name::RIGHT_BRACE);
	case '[':
		return MakeToken(Token::Name::LEFT_BRACKET);
	case '@':
		return MakeToken(Token::Name::AT);
	case '#':
		return MakeToken(Token::Name::HASH);
	case ']':
		return MakeToken(Token::Name::RIGHT_BRACKET);
	case ',':
		return MakeToken(Token::Name::COMMA);
	case '.':
		if (IsDigit(LookAhead(0)))
		{
			return MatchNumber();
		}
		else
		{
			return MakeToken(Token::Name::DOT);
		}
	case ';':
		return MakeToken(Token::Name::SINGLE_SEMICOLON);
	case '+':
		if (MatchNext('+'))
		{
			return MakeToken(Token::Name::DOUBLE_PLUS);
		}
		else
		{
			return MakeToken(Token::Name::SINGLE_PLUS);
		}
	case '-':
		if (MatchNext('>'))
		{
			return MakeToken(Token::Name::THIN_ARROW);
		}
		else
		{
			return MakeToken(Token::Name::MINUS);
		}
	case '?':
		return MakeToken(Token::Name::QUESTION);
	case ':':
		if (MatchNext(':'))
		{
			return MakeToken(Token::Name::DOUBLE_COLON);
		}
		else if (MatchNext('='))
		{
			return MakeToken(Token::Name::COLON_EQUAL);
		}
		else
		{
			return MakeToken(Token::Name::SINGLE_COLON);
		}
	case '%':
		if (MatchNext('>'))
		{
			return MakeToken(Token::Name::RIGHT_SHIFT);
		}
		else
		{
			return MakeToken(Token::Name::PERCENT);
		}
	case '*':
		return MakeToken(Token::Name::STAR);
	case '/':
		return MakeToken(Token::Name::SLASH);
	case '|':
		if (MatchNext('|'))
		{
			return MakeToken(Token::Name::DOUBLE_BAR);
		}
		else
		{
			return MakeToken(Token::Name::SINGLE_BAR);
		}
	case '^':
		return MakeToken(Token::Name::CARET);
	case '&':
		if (MatchNext('&'))
		{
			return MakeToken(Token::Name::DOUBLE_AMPERSAND);
		}
		else
		{
			return MakeToken(Token::Name::SINGLE_AMPERSAND);
		}
	case '!':
		if (MatchNext('='))
		{
			return MakeToken(Token::Name::BANG_EQUAL);
		}
		else
		{
			return MakeToken(Token::Name::BANG);
		}
	case '=':
		if (MatchNext('='))
		{
			return MakeToken(Token::Name::DOUBLE_EQUAL);
		}
		else
		{
			return MakeToken(Token::Name::SINGLE_EQUAL);
		}
	case '>':
		if (MatchNext('='))
		{
			return MakeToken(Token::Name::GREATER_EQUAL);
		}
		else
		{
			return MakeToken(Token::Name::RIGHT_ANGLE);
		}
	case '<':
		if (MatchNext('='))
		{
			return MakeToken(Token::Name::LESS_EQUAL);
		}
		else if (MatchNext('%'))
		{
			return MakeToken(Token::Name::LEFT_SHIFT);
		}
		else
		{
			return MakeToken(Token::Name::LEFT_ANGLE);
		}
	case '~':
		return MakeToken(Token::Name::TILDE);
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
			return std::unexpected<std::string>(MidoriError::GenerateLexerError("Invalid character.", m_line));
		}
	}
}

MidoriResult::LexerResult Lexer::Lex()
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
		return std::unexpected<std::vector<std::string>>(errors);
	}

	if (std::prev(token_stream.cend())->m_token_name != Token::Name::END_OF_FILE)
	{
		token_stream.AddToken(MakeToken(Token::Name::END_OF_FILE));
	}

	return token_stream;
}