#include "Compiler.h"
#include "Compiler/Lexer/Lexer.h"
#include "Compiler/Parser/Parser.h"
#include "Compiler/CodeGenerator/CodeGenerator.h"
#include "StaticAnalyzer/StaticAnalyzer.h"


#ifdef DEBUG
#include "Utility/AbstractSyntaxTreePrinter/AbstractSyntaxTreePrinter.h"
#include "Utility/Disassembler/Disassembler.h"
#endif

namespace Compiler
{
	std::optional<ExecutableModule> Compile(std::string&& script)
	{
		Lexer lexer(std::move(script));
		Lexer::LexerResult lexer_result = lexer.Lex();
		if (!lexer_result.m_error)
		{
			TokenStream tokens = std::move(lexer_result.m_tokens);
			Parser parser(std::move(tokens));
			Parser::ParserResult parser_result = parser.Parse();
			if (!parser_result.m_error)
			{
				ProgramTree program = std::move(parser_result.m_program);
				if (!StaticAnalyzer::AnalyzeProgram(program))
				{
					std::cerr << "Compilation failed due to static analysis error.\n";
					return std::nullopt;
				}
#ifdef DEBUG
				PrintAbstractSyntaxTree ast_printer;
				for (const std::unique_ptr<Statement>& statement : program)
				{
					std::visit(ast_printer, *statement);
				}
#endif
				CodeGenerator code_generator;
				CodeGenerator::CodeGeneratorResult bytecode = code_generator.GenerateCode(std::move(program));
				if (!bytecode.m_error)
				{

#ifdef DEBUG
					Disassembler::DisassembleBytecodeStream(bytecode.m_module, "main");
					for (const ExecutableModule* m : bytecode.m_sub_modules)
					{
						std::string function_name = std::string("Function at: ") + std::to_string(reinterpret_cast<uintptr_t>(m));
						Disassembler::DisassembleBytecodeStream(*m, function_name.data());
					}
#endif
					return std::move(bytecode.m_module);
				}
				else
				{
					std::cerr << "Compilation failed due to code generation error.\n";
				}
			}
			else
			{
				std::cerr << "Compilation failed due to parser error.\n";
			}
		}
		else
		{
			std::cerr << "Compilation failed due to lexer error.\n";
		}

		return std::nullopt;
	}
};
