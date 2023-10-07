#include "StaticAnalyzer.h"

#include <numeric>
#include <iostream>

namespace ReturnAnalyzer
{
	bool HasReturnStatement(const Statement& stmt)
	{
		return std::visit([](auto&& arg) -> bool
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, Return>)
				{
					return true;
				}
				else if constexpr (std::is_same_v<T, If>)
				{
					bool true_branch_result = HasReturnStatement(*arg.m_true_branch);

					if (!true_branch_result)
					{
						return false;
					}

					if (arg.m_else_branch.has_value())
					{
						return true_branch_result && HasReturnStatement(*arg.m_else_branch.value());
					}

					return false;
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
				else if constexpr (std::is_same_v<T, For>)
				{
					return HasReturnStatement(*arg.m_body);
				}
				else if constexpr (std::is_same_v<T, While>)
				{
					return HasReturnStatement(*arg.m_body);
				}
				else
				{
					return false;
				}
			}, stmt);
	}

	bool AnalyzeReturn(const Statement& stmt)
	{
		return std::visit([&](auto&& arg) -> bool
				{
					using ST = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<ST, Let>) {
						if (arg.m_value.has_value())
						{
							return std::visit([&arg](auto&& inner_arg) -> bool
								{
									using ET = std::decay_t<decltype(inner_arg)>;
									if constexpr (std::is_same_v<ET, Function>)
									{
										bool function_result = HasReturnStatement(*inner_arg.m_body);
										if (!function_result)
										{
											std::cerr << arg.m_name.m_lexeme << " does not return in all paths.\n";
										}
										
										const Block& body = std::get<Block>(*inner_arg.m_body);
										for (const std::unique_ptr<Statement>& function_body_stmt : body.m_stmts)
										{
											if (std::holds_alternative<Let>(*function_body_stmt))
											{
												function_result = function_result && AnalyzeReturn(*function_body_stmt);
											}
										}

										return function_result;
									}
									else
									{
										return true;
									}
								}, *arg.m_value.value());
						}
						else
						{
							return true;
						}
					}
					else if constexpr (std::is_same_v<ST, Block>)
					{
						return std::accumulate(arg.m_stmts.cbegin(), arg.m_stmts.cend(), true, [](bool acc, const std::unique_ptr<Statement>& stmt) -> bool
							{
								return acc && ReturnAnalyzer::AnalyzeReturn(*stmt);
							});
					}
					else if constexpr (std::is_same_v<ST, If>)
					{
						bool true_branch_result = AnalyzeReturn(*arg.m_true_branch);

						if (!true_branch_result)
						{
							return false;
						}
						else
						{
							if (arg.m_else_branch.has_value())
							{
								return AnalyzeReturn(*arg.m_else_branch.value());
							}

							return true;
						}
					}
					else if constexpr (std::is_same_v <ST, While>)
					{
						return AnalyzeReturn(*arg.m_body);
					}
					else if constexpr (std::is_same_v<ST, For>)
					{
						return AnalyzeReturn(*arg.m_condition_intializer) && AnalyzeReturn(*arg.m_body);
					}
					else
					{
						return true;
					}
				}, stmt);
	}
}

bool StaticAnalyzer::AnalyzeReturn(const ProgramTree& prog)
{
	return std::accumulate(prog.cbegin(), prog.cend(), true, [](bool acc, const std::unique_ptr<Statement>& stmt) -> bool
		{
			return acc && ReturnAnalyzer::AnalyzeReturn(*stmt);
		});
}
