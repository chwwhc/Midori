#include "Compiler/Compiler.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

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
	std::cout << "\033[0m"; // Reset color
	const char* file_name = "E:\\Projects\\Midori\\test\\test.mdr";

	std::optional<std::string> script = ReadFile(file_name);
	if (!script.has_value())
	{
		std::cerr << "Script is empty or could not be read." << std::endl;
		std::exit(60);
	}

	MidoriResult::CompilerResult compilation_result = Compiler::Compile(std::move(script.value()), std::string(file_name));
	if (compilation_result.has_value())
	{
		MidoriExecutable& compilation_result_value = compilation_result.value();
		VirtualMachine vm(std::move(compilation_result_value));
		vm.Execute();
	}
	else
	{
		std::cerr << "Compilation failed :( \n" << std::endl;
		std::cerr << compilation_result.error() << std::endl;
	}

	return 0;
}
