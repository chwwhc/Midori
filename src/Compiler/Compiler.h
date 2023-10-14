#pragma once

#include "Compiler/Error/Error.h"

namespace Compiler
{
	Result::CompilerResult Compile(std::string&& script);
};
