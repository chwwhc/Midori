#include <iostream>

#include "Compiler/Lexer/Lexer.h"
#include "Compiler/Parser/Parser.h"
#include "Utility/AbstractSyntaxTreePrinter/AbstractSyntaxTreePrinter.h"

int main()
{
	std::cout << "\033[32m";  // Set the text color to green
	std::cout << std::endl;
	std::cout << "	M   M III DDDD  OOO  RRRR  III" << std::endl;
	std::cout << "	MM MM  I  D   D O   O R   R  I" << std::endl;
	std::cout << "	M M M  I  D   D O   O RRRR   I" << std::endl;
	std::cout << "	M   M  I  D   D O   O R  R   I" << std::endl;
	std::cout << "	M   M III DDDD  OOO  R   R III" << std::endl;
	std::cout << std::endl;
	std::cout << "\033[0m";  // Reset the text color to default

	Lexer lexer("var x = [1,2,3];");
	try
	{
		TokenStream tokens = lexer.Lex();
		Parser parser(std::move(tokens));

		Program program = parser.Parse();

		AbstractSyntaxTreePrinter printer;

		for (const auto& statement : program)
		{
			std::visit(printer, *statement);
		}
	}
	catch (const CompilerError& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
