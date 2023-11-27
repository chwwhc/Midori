#include "NativeFunction.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"

#include <iostream>

void NativeFunction::InitializeNativeFunctions(VirtualMachine& vm)
{
	MidoriValue println = vm.AllocateObject(MidoriTraceable::NativeFunction([&vm]()
		{
			std::cout << vm.Pop().ToString() << std::endl;
			vm.Push(MidoriValue());
		}, "PrintLine"));
	vm.m_global_vars[std::string("PrintLine")] = println;
}
