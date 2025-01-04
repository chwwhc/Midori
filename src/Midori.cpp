#include <algorithm>
#include <fstream>
#include <sstream>

#include "Common/Printer/Printer.h"
#include "Compiler/Compiler.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"

std::string ReadFile(const char* filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
	{
		Printer::Print<Printer::Color::RED>(std::format("Could not open file: {}\n", filename));
		std::exit(EXIT_FAILURE);
	}

	std::ostringstream buffer;
	buffer << file.rdbuf();
	if (!buffer)
	{
		Printer::Print<Printer::Color::RED>(std::format("Could not read file to buffer: {}\n", filename));
		std::exit(EXIT_FAILURE);
	}

	return buffer.str();
}

int main(int argc, char* argv[])
{
	std::string file_name = "C:\\Users\\jk381\\source\\repos\\chwwhc\\Midori\\test\\test.mdr"s;
	std::string file_content = ReadFile(file_name.data());

	return Compiler::Compile(std::move(file_content), std::move(file_name))
		.and_then
		(
			[](MidoriExecutable&& executable)
			{
				VirtualMachine virtual_machine(std::move(executable));
				virtual_machine.Execute();
				return std::expected<int, std::string>(0);
			}
		)
		.or_else
		(
			[](std::string&& compilation_error)
			{
				Printer::Print<Printer::Color::RED>("Compilation failed :( \n");
				Printer::Print<Printer::Color::RED>(std::format("{}\n", compilation_error));
				return std::expected<int, std::string>(EXIT_FAILURE);
			}
		)
		.value();
}