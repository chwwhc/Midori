#include "NativeFunction.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"

#include <iostream>

void NativeFunction::InitializeNativeFunctions(VirtualMachine& vm)
{
	MidoriValue println = MidoriValue::NativeFunction([&vm]()
		{
			std::cout << vm.Pop().value()->ToString() << std::endl;
			vm.Push(MidoriValue());
		}, "PrintLine", 1);
	vm.m_global_vars[std::string("PrintLine")] = println;
}
