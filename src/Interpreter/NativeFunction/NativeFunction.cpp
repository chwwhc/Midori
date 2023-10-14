#include "NativeFunction.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"


void NativeFunction::InitializeNativeFunctions(VirtualMachine& vm)
{
	Object* print = Object::AllocateObject(Object::NativeFunction([&vm]()
		{
			std::cout << vm.Peek(0).ToString();
		}, "Print", 1));
	vm.m_global_vars[std::string("Print")] = print;
}
