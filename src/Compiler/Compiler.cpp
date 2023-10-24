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
				ProgramTree program = std::move(parser_result.value());
				MidoriResult::StaticAnalyzerResult static_analyzer_result = StaticAnalyzer::AnalyzeProgram(program);
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
					Disassembler::DisassembleBytecodeStream(compilation_result.value().m_modules[0], compilation_result.value().m_static_data, compilation_result.value().m_global_table, "main");
					for (size_t i = 1u; i < compilation_result.value().m_modules.size(); i += 1u)
					{
						const BytecodeStream& bytecode = compilation_result.value().m_modules[i];
						std::string function_name = std::string("Bytecode Stream at: ") + std::to_string(reinterpret_cast<uintptr_t>(&bytecode));
						Disassembler::DisassembleBytecodeStream(bytecode, compilation_result.value().m_static_data, compilation_result.value().m_global_table, function_name.data());
					}
#endif
					return compilation_result.value();
				}
			}
		}
	}
}