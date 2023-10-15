#pragma once

#include "Compiler/Error/Error.h"

class StaticAnalyzer
{
public:
	static Result::StaticAnalyzerResult AnalyzeProgram(const ProgramTree& prog)
	{
		return AnalyzeReturn(prog);
	}

private:
	static Result::StaticAnalyzerResult AnalyzeReturn(const ProgramTree& prog);
};
