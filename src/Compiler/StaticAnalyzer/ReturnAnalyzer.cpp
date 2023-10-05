#include "StaticAnalyzer.h"

#include <iostream>

namespace
{
	bool HasReturnStatement(const Statement& stmt) {
		return std::visit([](auto&& arg) -> bool {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, Return>) {
				return true;
			}
			else if constexpr (std::is_same_v<T, If>) {
				bool true_branch_result = HasReturnStatement(*arg.m_true_branch);

				if (!true_branch_result)
				{
					return false;
				}

				if (arg.m_else_branch.has_value())
				{
					return true_branch_result && HasReturnStatement(*arg.m_else_branch.value());
				}

				return true;
			}
			else if constexpr (std::is_same_v<T, Block>)
			{
				for (const std::unique_ptr<Statement>& statement : arg.m_stmts)
				{
					if (HasReturnStatement(*statement))
					{
						return true;
					}
				}

				return false;
			}
			else
			{
				return false;
			}
			}, stmt);
	}
}

bool StaticAnalyzer::AnalyzeReturn(const ProgramTree& prog)
{
	bool result = true;

	for (const std::unique_ptr<Statement>& stmt : prog)
	{
		result = result && std::visit([&](auto&& arg) -> bool {
			using ST = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<ST, Let>) {
				if (arg.m_value.has_value()) {
					return std::visit([&arg](auto&& inner_arg) -> bool {
						using ET = std::decay_t<decltype(inner_arg)>;
						if constexpr (std::is_same_v<ET, Function>) {
							bool functionResult = HasReturnStatement(*inner_arg.m_body);
							if (!functionResult)
							{
								std::cerr << arg.m_name.m_lexeme << " does not return in all paths.\n";
							}
							return functionResult;
						}
						else {
							return true;
						}
						}, *arg.m_value.value());
				}
				else {
					return true;
				}
			}
			else {
				return true;
			}
			}, *stmt);
	}

	return result;
}
