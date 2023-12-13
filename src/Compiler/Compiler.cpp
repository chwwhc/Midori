#include "Compiler.h"
#include "Compiler/Lexer/Lexer.h"
#include "Compiler/Parser/Parser.h"
#include "Compiler/CodeGenerator/CodeGenerator.h"
#include "TypeChecker/TypeChecker.h"


#ifdef DEBUG
#include "Utility/AbstractSyntaxTreePrinter/AbstractSyntaxTreePrinter.h"
#include "Utility/Disassembler/Disassembler.h"
#endif

namespace Compiler
{
	MidoriResult::CompilerResult Compile(std::string&& script)
	{
		Lexer lexer(std::move(script));
		MidoriResult::LexerResult lexer_result = lexer.Lex();
		if (!lexer_result.has_value())
		{
			return std::unexpected<std::vector<std::string>>(std::move(lexer_result.error()));
		}
		else
		{
			TokenStream tokens = std::move(lexer_result.value());
			Parser parser(std::move(tokens));
			MidoriResult::ParserResult parser_result = parser.Parse();
			if (!parser_result.has_value())
			{
				return std::unexpected<std::vector<std::string>>(std::move(parser_result.error()));
			}
			else
			{
				TypeChecker type_checker;
				MidoriProgramTree program = std::move(parser_result.value());
				MidoriResult::TypeCheckerResult type_checker_result = type_checker.TypeCheck(program);
				if (type_checker_result.has_value())
				{
					return std::unexpected<std::vector<std::string>>(std::move(type_checker_result.value()));
				}
#ifdef DEBUG
				PrintAbstractSyntaxTree ast_printer;
				for (const std::unique_ptr<MidoriStatement>& statement : program)
				{
					std::visit(ast_printer, *statement);
				}
#endif
				CodeGenerator code_generator;
				MidoriResult::CodeGeneratorResult compilation_result = code_generator.GenerateCode(std::move(program));
				if (!compilation_result.has_value())
				{
					return std::unexpected<std::vector<std::string>>(std::move(compilation_result.error()));
				}
				else
				{
#ifdef DEBUG
					const MidoriExecutable& executable = compilation_result.value();
					for (size_t i = 0u; i < executable.m_procedure_names.size(); i += 1u)
					{
						const BytecodeStream& bytecode = executable.GetBytecodeStream(static_cast<int>(i));
						std::string variable_name = executable.m_procedure_names[i];
						Disassembler::DisassembleBytecodeStream(executable, static_cast<int>(i), variable_name);
					}
#endif
					return compilation_result.value();
				}
			}
		}
	}
}