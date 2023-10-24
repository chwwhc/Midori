#include "NativeFunction.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"


void NativeFunction::InitializeNativeFunctions(VirtualMachine& vm)
{
	Value println = Value::NativeFunction([&vm]()
		{
			std::cout << vm.Pop().value()->ToString() << std::endl;
			vm.Push(Value());
		}, "PrintLine", 1);
	vm.m_global_vars[std::string("PrintLine")] = println;
}
