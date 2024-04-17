#include "AbstractSyntaxTree.h"

bool Switch::IsMemberCase(const Case& c)
{
	return std::holds_alternative<MemberCase>(c);
}

bool Switch::IsDefaultCase(const Case& c)
{
	return std::holds_alternative<DefaultCase>(c);
}

Switch::MemberCase& Switch::GetMemberCase(const Case& c)
{
	return const_cast<MemberCase&>(std::get<MemberCase>(c));
}

Switch::DefaultCase& Switch::GetDefaultCase(const Case& c)
{
	return const_cast<DefaultCase&>(std::get<DefaultCase>(c));
}

const Token& Switch::GetKeyword(const Case& c)
{
	return std::visit([](auto&& arg) -> const Token&
		{
			return arg.m_keyword;
		}, c);
}

const std::unique_ptr<MidoriStatement>& Switch::GetCaseStatement(const Case& c)
{
	return std::visit([](auto&& arg) -> const std::unique_ptr<MidoriStatement>&
		{
			return arg.m_stmt;
		}, c);
}
