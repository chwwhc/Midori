#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"

class StaticAnalyzer
{
public:
	static MidoriResult::StaticAnalyzerResult AnalyzeProgram(const ProgramTree& prog)
	{
		std::vector<std::string> result = AnalyzeReturn(prog);
		if (result.empty())
		{
			return std::nullopt;
		}
		else
		{
			return result;
		}
	}

private:
	static std::vector<std::string> AnalyzeReturn(const ProgramTree& prog);
};
