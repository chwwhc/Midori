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
	const char* filename = "C:\\Users\\JK\\Documents\\GitHub\\Midori\\test\\test.mdr";

	std::optional<std::string> script = ReadFile(filename);
	if (!script.has_value())
	{
		std::cerr << "Script is empty or could not be read." << std::endl;
		std::exit(60);
	}

	MidoriResult::CompilerResult compilation_result = Compiler::Compile(std::move(script.value()));
	if (compilation_result.has_value())
	{
		MidoriExecutable& compilation_result_value = compilation_result.value();
		VirtualMachine vm(std::move(compilation_result_value));
		vm.Execute();
	}
	else
	{
		std::for_each(compilation_result.error().cbegin(), compilation_result.error().cend(), [](const std::string& error) { std::cerr << error << std::endl; });
	}

	return 0;
}
