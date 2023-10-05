#pragma once

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"

class StaticAnalyzer
{
public:
	static bool AnalyzeProgram(const ProgramTree& prog)
	{
		return AnalyzeReturn(prog);
	}

private:
	static bool AnalyzeReturn(const ProgramTree& prog);
};
