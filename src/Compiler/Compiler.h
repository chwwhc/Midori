#pragma once

#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Common/Value/StaticData.h"

#include <optional>

namespace Compiler
{

	struct ExecutableModule
	{
		BytecodeStream m_bytecode;
		Traceable::GarbageCollectionRoots m_constant_roots;
		StaticData m_static_data;
		GlobalVariableTable m_global_table;
	};

	std::optional<ExecutableModule> Compile(std::string&& script);
};
