#include "Compiler/Compiler.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"

#include <fstream>
#include <sstream>
#include <iostream>

std::optional<std::string> ReadFile(const char* filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
	{
		std::cerr << "Could not open file: " << filename << std::endl;
		return std::nullopt;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}


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

	const char* filename = "E:\\Projects\\Midori\\test\\test.mdr";

	std::optional<std::string> script = ReadFile(filename);
	if (!script.has_value())
	{
		std::cerr << "Script is empty or could not be read." << std::endl;
		std::exit(60);
	}

	std::optional<ExecutableModule> module = Compiler::Compile(std::move(script.value()));
	if (module.has_value())
	{
		VirtualMachine vm(std::move(module.value()));
		vm.Execute();
	}

	return 0;
}
