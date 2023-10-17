#pragma once

#include "Compiler/Error/Error.h"

class StaticAnalyzer
{
public:
	static Result::StaticAnalyzerResult AnalyzeProgram(const ProgramTree& prog)
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
