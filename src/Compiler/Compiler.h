#pragma once

#include "Common/ExecutableModule/ExecutableModule.h"

#include <optional>

namespace Compiler
{
	std::optional<ExecutableModule> Compile(std::string&& script);
};
