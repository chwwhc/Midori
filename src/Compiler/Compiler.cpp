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
	Result::CompilerResult Compile(std::string&& script)
	{
		Lexer lexer(std::move(script));
		Result::LexerResult lexer_result = lexer.Lex();
		if (!lexer_result.has_value())
		{
			return std::unexpected(std::move(lexer_result.error()));
		}
		else
		{
			TokenStream tokens = std::move(lexer_result.value());
			Parser parser(std::move(tokens));
			Result::ParserResult parser_result = parser.Parse();
			if (!parser_result.has_value())
			{
				return std::unexpected(std::move(parser_result.error()));
			}
			else
			{
				ProgramTree program = std::move(parser_result.value());
				Result::StaticAnalyzerResult static_analyzer_result = StaticAnalyzer::AnalyzeProgram(program);
				if (static_analyzer_result.has_value())
				{
					return std::unexpected(std::move(static_analyzer_result.value()));
				}
#ifdef DEBUG
				PrintAbstractSyntaxTree ast_printer;
				for (const std::unique_ptr<Statement>& statement : program)
				{
					std::visit(ast_printer, *statement);
				}
#endif
				CodeGenerator code_generator;
				Result::CodeGeneratorResult compilation_result = code_generator.GenerateCode(std::move(program));
				if (!compilation_result.has_value())
				{
					return std::unexpected(std::move(compilation_result.error()));
				}
				else
				{
#ifdef DEBUG
					Disassembler::DisassembleBytecodeStream(compilation_result.value().m_module, compilation_result.value().m_static_data, compilation_result.value().m_global_table, "main");
					for (const Object::DefinedFunction* m : compilation_result.value().m_sub_modules)
					{
						std::string function_name = std::string("Function at: ") + std::to_string(reinterpret_cast<uintptr_t>(m));
						Disassembler::DisassembleBytecodeStream(m->m_bytecode, compilation_result.value().m_static_data, compilation_result.value().m_global_table, function_name.data());
					}
#endif
					return { { std::move(compilation_result.value().m_module), std::move(compilation_result.value().m_constant_roots), std::move(compilation_result.value().m_static_data), std::move(compilation_result.value().m_global_table)} };
				}
			}
		}
	}
}