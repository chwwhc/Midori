#include "Compiler/Compiler.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

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

	const char* lines[] = 
	{
		"	MM MM III DDDD   OOO  RRRR  III ",
		"	MM MM  I  D   D O   O R   R  I  ",
		"	MM MM  I  D   D O   O RRRR   I  ",
		"	M M M  I  D   D O   O R  R   I  ",
		"	M   M III DDDD   OOO  R   R III "
	};

	for (int i = 0; i < 5; i += 1) 
	{
		std::cout << lines[i] << std::setw(10) << "|";
		if (i == 2) 
		{
			std::cout << "\t緑 (MIDORI) Language REPL";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
	std::cout << "\033[0m";  // Reset the text color to default

	const char* filename = "E:\\Projects\\Midori\\test\\test.mdr";

	std::optional<std::string> script = ReadFile(filename);
	if (!script.has_value())
	{
		std::cerr << "Script is empty or could not be read." << std::endl;
		std::exit(60);
	}

	auto compilation_result = Compiler::Compile(std::move(script.value()));
	if (compilation_result.has_value())
	{
		Result::ExecutableModule& compilation_result_value = compilation_result.value();
		VirtualMachine vm(std::move(compilation_result_value));
		vm.Execute();
	}
	else
	{
		for (const std::string& error : compilation_result.error())
		{
			std::cerr << error << std::endl;
		}
	}

	return 0;
}
