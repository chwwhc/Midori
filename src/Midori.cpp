#include "Compiler/Compiler.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"
#include "Common/Printer/Printer.h"

#include <fstream>
#include <sstream>
#include <algorithm>

std::optional<std::string> ReadFile(const char* filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
	{
		Printer::Print<Printer::Color::RED>("Could not open file: \n");
		return std::nullopt;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

int main()
{
	const char* file_name = "E:\\Projects\\Midori\\test\\test.mdr";

	std::optional<std::string> script = ReadFile(file_name);
	if (!script.has_value())
	{
		Printer::Print<Printer::Color::RED>("Could not read file: \n");
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
		Printer::Print<Printer::Color::RED>("Compilation failed :( \n");
		Printer::Print<Printer::Color::RED>(compilation_result.error() + '\n');
	}

	return 0;
}
