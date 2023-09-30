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
		"class X < Y {"
		"init() {"
		"let x = 1;"
		"let y = false;"
		"let z = \"hello\";"
		"let a = nil;"
		"super.init();"
		"}"
		"}";

	Lexer lexer(std::move(script_3));
	Lexer::LexerResult lexer_result = lexer.Lex();
	if (!lexer_result.m_error)
	{
		TokenStream tokens = std::move(lexer_result.m_tokens);
		Parser parser(std::move(tokens));
		Parser::ParserResult parser_result = parser.Parse();
		if (!parser_result.m_error)
		{
			Program program = std::move(parser_result.m_program);
			AbstractSyntaxTreePrinter printer;
			for (const auto& statement : program)
			{
				std::visit(printer, *statement);
			}

		}
	}

	return 0;
}
