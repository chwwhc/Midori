#pragma once

class VirtualMachine;

class NativeFunction
{
public:
	static void InitializeNativeFunctions(VirtualMachine& vm);
};