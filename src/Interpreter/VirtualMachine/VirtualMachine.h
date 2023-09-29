#pragma once

#include <array>

#include "Common/Value/Value.h"

namespace
{
	constexpr int STACK_MAX = 512;
}

class VirtualMachine
{
public:
	void Execute();

private:

	

public:

private:
	std::array<Value, STACK_MAX> m_value_stack;
	Value* m_stack_pointer;
	
};
