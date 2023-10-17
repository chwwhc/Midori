#include "NativeFunction.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"


void NativeFunction::InitializeNativeFunctions(VirtualMachine& vm)
{
	Object* println = Object::AllocateObject(Object::NativeFunction([&vm]()
		{
			std::cout << vm.Peek(0).m_value.ToString() << std::endl;
		}, "PrintLine", 1));
	vm.m_global_vars[std::string("PrintLine")] = println;
}
