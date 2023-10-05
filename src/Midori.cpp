#include "Compiler/Compiler.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"

#include <fstream>
#include <iostream>


int main()
{
	std::cout << "\033[32m";  // Set the text color to green
	std::cout << std::endl;
	std::cout << "	MM MM III DDDD   OOO  RRRR  III" << std::endl;
	std::cout << "	MM MM  I  D   D O   O R   R  I  " << std::endl;
	std::cout << "	MM MM  I  D   D O   O RRRR   I  " << std::endl;
	std::cout << "	M M M  I  D   D O   O R  R   I  " << std::endl;
	std::cout << "	M   M III DDDD   OOO  R   R III " << std::endl;
	std::cout << std::endl;
	std::cout << "\033[0m";  // Reset the text color to default

	std::string script_1 = "let x = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]; "
		"let y = [9, 8, 7, 6, 5, 4, 3, 2, 1, 0];"

		"for (let i = 0; i < 10; i = i + 1)"
		"{"
		"for (let j = 0; j < 10; j = j + 1)"
		"{"
		"print x[i];"
		"print y[j];"
		"}"
		"}";

	std::string script_2 = "fn add(a, b) "
		"{"
		"return a + b;"
		"}"
		"let x = add(1, 2);"
		"print x;";

	std::string script_3 =
		"let fun = \\ x -> {Print(x);};"
		"let f = 1;"
		"fun(f);";

	std::optional<ExecutableModule> module = Compiler::Compile(std::move(script_3));
	if (module.has_value())
	{
		VirtualMachine vm(std::move(module.value()));
		vm.Execute();
	}

	return 0;
}
