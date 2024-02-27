#include "Compiler/Compiler.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"
#include "Common/Printer/Printer.h"

#include <fstream>
#include <sstream>
#include <algorithm>

std::optional<std::string> ReadFile(const char* filename) noexcept
{
	std::ifstream file(filename);
	if (!file.is_open())
	{
		Printer::Print<Printer::Color::RED>("Could not open file: \n");
		return std::nullopt;
	}

	std::ostringstream buffer;
	buffer << file.rdbuf();
	return std::optional(buffer.str());
}

int main() noexcept
{
	std::string file_name = "E:\\Projects\\Midori\\test\\test.mdr"s;

	std::optional<std::string> code = ReadFile(file_name.data());
	if (!code.has_value())
	{
		Printer::Print<Printer::Color::RED>("Could not read file: \n");
		std::exit(EXIT_FAILURE);
	}

	MidoriResult::CompilerResult compilation_result = Compiler::Compile(std::move(code.value()), std::move(file_name));
	if (compilation_result.has_value())
	{
		MidoriExecutable& compilation_result_value = compilation_result.value();
		VirtualMachine vm(std::move(compilation_result_value));
		vm.Execute();
	}
	else
	{
		Printer::Print<Printer::Color::RED>("Compilation failed :( \n");
		Printer::Print<Printer::Color::RED>(std::format("{}\n", compilation_result.error()));
		std::exit(EXIT_FAILURE);
	}

	return 0;
}
