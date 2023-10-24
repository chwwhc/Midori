#include "StaticAnalyzer.h"

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

	void AnalyzeReturn(const Statement& stmt, std::vector<std::string>& errors)
	{
		return std::visit([&stmt, &errors](auto&& arg) -> void
			{
				using ST = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<ST, Define>) {
					return std::visit([&stmt, &errors, &arg](auto&& inner_arg) -> void
						{
							using ET = std::decay_t<decltype(inner_arg)>;
							if constexpr (std::is_same_v<ET, Closure>)
							{
								bool function_result = HasReturnStatement(*inner_arg.m_body);
								if (!function_result)
								{
									errors.emplace_back(MidoriError::GenerateStaticAnalyzerError("Does not return in all paths.", arg.m_name));
								}

								const Block& body = std::get<Block>(*inner_arg.m_body);
								for (const std::unique_ptr<Statement>& function_body_stmt : body.m_stmts)
								{
									if (std::holds_alternative<Define>(*function_body_stmt))
									{
										AnalyzeReturn(*function_body_stmt, errors);
									}
								}
							}
						}, *arg.m_value);
				}
				else if constexpr (std::is_same_v<ST, Block>)
				{
					for (const std::unique_ptr<Statement>& stmt : arg.m_stmts)
					{
						ReturnAnalyzer::AnalyzeReturn(*stmt, errors);
					}
				}
				else if constexpr (std::is_same_v<ST, If>)
				{
					AnalyzeReturn(*arg.m_true_branch, errors);

					if (arg.m_else_branch.has_value())
					{
						AnalyzeReturn(*arg.m_else_branch.value(), errors);
					}

				}
				else if constexpr (std::is_same_v <ST, While>)
				{
					AnalyzeReturn(*arg.m_body, errors);
				}
				else if constexpr (std::is_same_v<ST, For>)
				{
					AnalyzeReturn(*arg.m_condition_intializer, errors);
					AnalyzeReturn(*arg.m_body, errors);
				}
			}, stmt);
	}
}

std::vector<std::string> StaticAnalyzer::AnalyzeReturn(const ProgramTree& prog)
{
	std::vector<std::string> errors;
	for (const std::unique_ptr<Statement>& stmt : prog)
	{
		ReturnAnalyzer::AnalyzeReturn(*stmt, errors);
	}

	return errors;
}
