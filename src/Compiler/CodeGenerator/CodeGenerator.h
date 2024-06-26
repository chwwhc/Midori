#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"

#include <algorithm>
#include <stack>

class CodeGenerator
{
public:

private:
	struct MainProcedureContext
	{
		int m_main_procedure_index = 0;
		int m_main_procedure_global_table_index = 0;
		int m_main_procedure_arity = 0;
		int m_main_procedure_line = 0;
	};

	struct LoopContext
	{
		std::vector<int> m_break_positions;
		int m_loop_start = 0;
	};

	MidoriExecutable::Procedures m_procedures{ BytecodeStream() };
#ifdef DEBUG
	std::vector<MidoriText> m_procedure_names{ MidoriText("runtime startup") };
#endif
	std::string m_errors;
	std::stack<LoopContext> m_loop_contexts;
	std::unordered_map<std::string, int> m_global_variables;

	MidoriExecutable m_executable;
	std::optional<MainProcedureContext> m_main_function_ctx = std::nullopt;
	int m_current_procedure_index = 0;
	OpCode m_last_opcode = OpCode::HALT;

public:

	MidoriResult::CodeGeneratorResult GenerateCode(MidoriProgramTree&& program_tree);

private:

	template<typename T>
	requires std::is_same_v<T, std::unique_ptr<MidoriExpression>&> || std::is_same_v<T, std::unique_ptr<MidoriStatement>&>
	void EmitNumericConditionalJump(ConditionOperandType operand_type, T true_branch, T else_branch, int line)
	{
		int if_jump;
		if (operand_type == ConditionOperandType::INTEGER)
		{
			PopByte(line);
			switch (m_last_opcode)
			{
			case OpCode::LESS_INTEGER:
				if_jump = EmitJump(OpCode::IF_INTEGER_LESS, line);
				break;
			case OpCode::LESS_EQUAL_INTEGER:
				if_jump = EmitJump(OpCode::IF_INTEGER_LESS_EQUAL, line);
				break;
			case OpCode::GREATER_INTEGER:
				if_jump = EmitJump(OpCode::IF_INTEGER_GREATER, line);
				break;
			case OpCode::GREATER_EQUAL_INTEGER:
				if_jump = EmitJump(OpCode::IF_INTEGER_GREATER_EQUAL, line);
				break;
			case OpCode::EQUAL_INTEGER:
				if_jump = EmitJump(OpCode::IF_INTEGER_EQUAL, line);
				break;
			case OpCode::NOT_EQUAL_INTEGER:
				if_jump = EmitJump(OpCode::IF_INTEGER_NOT_EQUAL, line);
				break;
			default:
				AddError(MidoriError::GenerateCodeGeneratorError("Invalid opcode for integer ternary condition.", line));
				return;
			}
			std::visit([this](auto&& arg)
				{
					(*this)(arg);
				}, *true_branch);
			int else_jump = EmitJump(OpCode::JUMP, line);
			PatchJump(if_jump, line);
			if (else_branch != nullptr)
			{
				std::visit([this](auto&& arg)
					{
						(*this)(arg);
					}, *else_branch);
			}
			PatchJump(else_jump, line);
		}
		else
		{
			PopByte(line);
			switch (m_last_opcode)
			{
			case OpCode::LESS_FRACTION:
				if_jump = EmitJump(OpCode::IF_FRACTION_LESS, line);
				break;
			case OpCode::LESS_EQUAL_FRACTION:
				if_jump = EmitJump(OpCode::IF_FRACTION_LESS_EQUAL, line);
				break;
			case OpCode::GREATER_FRACTION:
				if_jump = EmitJump(OpCode::IF_FRACTION_GREATER, line);
				break;
			case OpCode::GREATER_EQUAL_FRACTION:
				if_jump = EmitJump(OpCode::IF_FRACTION_GREATER_EQUAL, line);
				break;
			case OpCode::EQUAL_FRACTION:
				if_jump = EmitJump(OpCode::IF_FRACTION_EQUAL, line);
				break;
			case OpCode::NOT_EQUAL_FRACTION:
				if_jump = EmitJump(OpCode::IF_FRACTION_NOT_EQUAL, line);
				break;
			default:
				AddError(MidoriError::GenerateCodeGeneratorError("Invalid opcode for fraction ternary condition.", line));
				return;
			}
			std::visit([this](auto&& arg)
				{
					(*this)(arg);
				}, *true_branch);
			int else_jump = EmitJump(OpCode::JUMP, line);
			PatchJump(if_jump, line);
			if (else_branch != nullptr)
			{
				std::visit([this](auto&& arg)
					{
						(*this)(arg);
					}, *else_branch);
			}
			PatchJump(else_jump, line);
		}
	}

	void AddError(std::string&& error);

	void PopByte(int line);

	void EmitByte(OpCode byte, int line);

	void EmitTwoBytes(int byte1, int byte2, int line);

	void EmitThreeBytes(int byte1, int byte2, int byte3, int line);

	void EmitNumericConstant(MidoriInteger val, int line, bool is_integer);

	void EmitConstant(MidoriValue&& value, int line);

	void EmitVariable(int variable_index, OpCode op, int line);

	int EmitJump(OpCode op, int line);

	void PatchJump(int offset, int line);

	void EmitLoop(int loop_start, int line);

	void BeginLoop(int loop_start);

	void EndLoop(int line);

	void operator()(Block& block);

	void operator()(Simple& simple);

	void operator()(Define& def);

	void operator()(If& if_stmt);

	void operator()(While& while_stmt);

	void operator()(For& for_stmt);

	void operator()(Break& break_stmt);

	void operator()(Continue& continue_stmt);

	void operator()(Return& return_stmt);

	void operator()(Foreign& foreign);

	void operator()(Struct& struct_stmt);

	void operator()(Union& union_stmt);

	void operator()(Switch& switch_stmt);

	void operator()(Namespace& namespace_stmt);

	void operator()(As& as);

	void operator()(Binary& binary);

	void operator()(Group& group);

	void operator()(UnaryPrefix& unary);

	void operator()(UnarySuffix& unary);

	void operator()(Call& call);

	void operator()(Get& get);

	void operator()(Set& set);

	void operator()(Variable& variable);

	void operator()(Bind& bind);

	void operator()(TextLiteral& text);

	void operator()(BoolLiteral& bool_expr);

	void operator()(FractionLiteral& fraction);

	void operator()(IntegerLiteral& integer);

	void operator()(UnitLiteral& unit);

	void operator()(Closure& closure);

	void operator()(Construct& construct);

	void operator()(Array& array);

	void operator()(ArrayGet& array_get);

	void operator()(ArraySet& array_set);

	void operator()(Ternary& ternary);
};
