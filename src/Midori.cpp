#include "Compiler/Lexer/Lexer.h"
#include "Compiler/Parser/Parser.h"
#include "Compiler/CodeGenerator/CodeGenerator.h"
#include "Utility/AbstractSyntaxTreePrinter/AbstractSyntaxTreePrinter.h"
#include "Utility/Disassembler/Disassembler.h"
#include "Common/ExecutableModule/ExecutableModule.h"
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
		"let x = 2; \n for(let x = 1; x < 10; x = x + 1) {print x;}\n";

	Lexer lexer(std::move(script_3));
	Lexer::LexerResult lexer_result = lexer.Lex();
	if (!lexer_result.m_error)
	{
		TokenStream tokens = std::move(lexer_result.m_tokens);
		Parser parser(std::move(tokens));
		Parser::ParserResult parser_result = parser.Parse();
		if (!parser_result.m_error)
		{
			ProgramTree program = std::move(parser_result.m_program);

			PrintAbstractSyntaxTree ast_printer;
			for (const std::unique_ptr<Statement>& statement : program)
			{
				std::visit(ast_printer, *statement);
			}

			CodeGenerator code_generator;
			CodeGenerator::CodeGeneratorResult bytecode = code_generator.GenerateCode(std::move(program));
			if (!bytecode.m_error)
			{
				bytecode.m_module.AddByteCode(OpCode::RETURN, 2);
				Disassembler::DisassembleBytecodeStream(bytecode.m_module, "main");

				VirtualMachine vm(std::move(bytecode.m_module));
				vm.Execute();
			}
		}
	}

	return 0;
}
