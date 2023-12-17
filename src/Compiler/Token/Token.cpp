#include "Token.h"

TokenStream::iterator TokenStream::begin()
{ 
	return m_tokens.begin(); 
}

TokenStream::iterator TokenStream::end()
{ 
	return m_tokens.end();
}

TokenStream::const_iterator TokenStream::cbegin() const
{ 
	return m_tokens.cbegin(); 
}

TokenStream::const_iterator TokenStream::cend() const
{ 
	return m_tokens.cend(); 
}

void TokenStream::AddToken(Token&& token) 
{ 
	m_tokens.emplace_back(std::move(token)); 
}

Token& TokenStream::operator[](int index) const 
{ 
	return const_cast<Token&>(m_tokens[index]);
}

int TokenStream::Size() const 
{ 
	return static_cast<int>(m_tokens.size()); 
}

void TokenStream::Insert(TokenStream::iterator iter, TokenStream&& tokens)
{
	m_tokens.insert(iter, tokens.begin(), tokens.end());
}

void TokenStream::Erase(TokenStream::iterator iter)
{
	m_tokens.erase(m_tokens.begin(), iter);
}
