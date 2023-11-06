#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"

class StaticAnalyzer
{
public:
	static MidoriResult::StaticAnalyzerResult AnalyzeProgram(const ProgramTree& prog)
	{
		return std::nullopt;
	}

private:
	static std::vector<std::string> AnalyzeReturn(const ProgramTree& prog);
};
