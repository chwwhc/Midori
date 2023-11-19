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
				StaticAnalyzer static_analyzer;
				ProgramTree program = std::move(parser_result.value());
				MidoriResult::StaticAnalyzerResult static_analyzer_result = static_analyzer.AnalyzeProgram(program);
				if (static_analyzer_result.has_value())
				{
					return std::unexpected<std::vector<std::string>>(std::move(static_analyzer_result.value()));
				}
#ifdef DEBUG
				PrintAbstractSyntaxTree ast_printer;
				for (const std::unique_ptr<Statement>& statement : program)
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
					const MidoriResult::ExecutableModule& executable_module = compilation_result.value();
					for (size_t i = 0u; i < executable_module.m_modules.size(); i += 1u)
					{
						const BytecodeStream& bytecode = executable_module.m_modules[i];
						std::string variable_name = executable_module.m_module_names[i];
						Disassembler::DisassembleBytecodeStream(bytecode, executable_module.m_static_data, executable_module.m_global_table, variable_name.data());
					}
#endif
					return compilation_result.value();
				}
			}
		}
	}
}