#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"

namespace Compiler
{
	MidoriResult::CompilerResult Compile(std::string&& script);
};
