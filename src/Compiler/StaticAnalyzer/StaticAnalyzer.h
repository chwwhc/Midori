#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"
#include "Compiler/StaticAnalyzer/TypeChecker/TypeChecker.h"

class StaticAnalyzer
{
private:
	TypeChecker m_type_checker;

public:
	MidoriResult::StaticAnalyzerResult AnalyzeProgram(ProgramTree& prog)
	{
		return m_type_checker.TypeCheck(prog);
	}

private:
};
