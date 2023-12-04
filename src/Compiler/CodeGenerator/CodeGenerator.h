#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"

#include <algorithm>
#include <stack>

class CodeGenerator
{
public:

private:
	struct MainModuleContext
	{
		int m_main_module_index = 0;
		int m_main_module_global_table_index = 0;
		int m_main_module_arity = 0;
		int m_main_module_line = 0;
	};

	struct LoopContext
	{
		std::vector<int> m_break_positions;
		int m_loop_start = 0;
	};

	std::vector<BytecodeStream> m_modules = { BytecodeStream() };
#ifdef DEBUG
	std::vector<std::string> m_module_names = { "runtime startup" };
#endif
	std::vector<std::string> m_errors;
	std::stack<LoopContext> m_loop_contexts;
	std::unordered_map<std::string, int> m_global_variables;

	StaticData m_static_data;
	GlobalVariableTable m_global_table;
	MidoriTraceable::GarbageCollectionRoots m_traceable_constants;
	std::optional<MainModuleContext> m_main_module_ctx = std::nullopt;
	int m_current_module_index = 0;

public:

	MidoriResult::CodeGeneratorResult GenerateCode(MidoriProgramTree&& program_tree);

private:

	inline void EmitByte(OpCode byte, int line) { m_modules[m_current_module_index].AddByteCode(byte, line); }

	inline void EmitTwoBytes(int byte1, int byte2, int line)
	{
		EmitByte(static_cast<OpCode>(byte1), line);
		EmitByte(static_cast<OpCode>(byte2), line);
	}

	inline void EmitThreeBytes(int byte1, int byte2, int byte3, int line)
	{
		EmitByte(static_cast<OpCode>(byte1), line);
		EmitByte(static_cast<OpCode>(byte2), line);
		EmitByte(static_cast<OpCode>(byte3), line);
	}

	inline void EmitConstant(MidoriValue&& value, int line)
	{
		/*
		if (value.IsObjectPointer())
		{
			MidoriTraceable* traceable = static_cast<MidoriTraceable*>(value.GetObjectPointer());
			m_traceable_constants.emplace(traceable);
			MidoriTraceable::s_static_bytes_allocated += traceable->GetSize();
		}
		*/

		int index = m_static_data.AddConstant(std::move(value));

		if (index <= UINT8_MAX) // 1 byte
		{
			EmitByte(OpCode::CONSTANT, line);
			EmitByte(static_cast<OpCode>(index), line);
		}
		else if (index <= UINT16_MAX) // 2 bytes
		{
			EmitByte(OpCode::CONSTANT_LONG, line);
			EmitByte(static_cast<OpCode>(index & 0xff), line);
			EmitByte(static_cast<OpCode>((index >> 8) & 0xff), line);
		}
		else if (index <= ((UINT16_MAX << 8) | 0xffff)) // 3 bytes
		{
			EmitByte(OpCode::CONSTANT_LONG_LONG, line);
			EmitByte(static_cast<OpCode>(index & 0xff), line);
			EmitByte(static_cast<OpCode>((index >> 8) & 0xff), line);
			EmitByte(static_cast<OpCode>((index >> 16) & 0xff), line);
		}
		else
		{
			m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too many constants (max 16777215).", line));
		}
	}

	inline void EmitVariable(int variable_index, OpCode op, int line)
	{
		if (variable_index > UINT8_MAX)
		{
			m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too many variables (max 255).", line));
		}

		EmitByte(op, line);
		EmitByte(static_cast<OpCode>(variable_index), line);
	}

	inline int EmitJump(OpCode op, int line)
	{
		EmitByte(op, line);
		EmitByte(static_cast<OpCode>(0xff), line);
		EmitByte(static_cast<OpCode>(0xff), line);
		return m_modules[m_current_module_index].GetByteCodeSize() - 2;
	}

	inline void PatchJump(int offset, int line)
	{
		int jump = m_modules[m_current_module_index].GetByteCodeSize() - offset - 2;
		if (jump > UINT16_MAX)
		{
			m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Too much code to jump over (max 65535).", line));
		}

		m_modules[m_current_module_index].SetByteCode(offset, static_cast<OpCode>(jump & 0xff));
		m_modules[m_current_module_index].SetByteCode(offset + 1, static_cast<OpCode>((jump >> 8) & 0xff));
	}

	inline void EmitLoop(int loop_start, int line)
	{
		EmitByte(OpCode::JUMP_BACK, line);

		int offset = m_modules[m_current_module_index].GetByteCodeSize() - loop_start + 2;
		if (offset > UINT16_MAX)
		{
			m_errors.emplace_back(MidoriError::GenerateCodeGeneratorError("Loop body too large (max 65535).", line));
		}

		EmitByte(static_cast<OpCode>(offset & 0xff), line);
		EmitByte(static_cast<OpCode>((offset >> 8) & 0xff), line);
	}

	inline void BeginLoop(int loop_start) { m_loop_contexts.emplace(std::vector<int>(), loop_start); }

	inline void EndLoop(int line)
	{
		LoopContext loop = m_loop_contexts.top();
		m_loop_contexts.pop();
		std::for_each(loop.m_break_positions.begin(), loop.m_break_positions.end(), [line, this](int break_position) { PatchJump(break_position, line); });
	}

	void operator()(Block& block);

	void operator()(Simple& simple);

	void operator()(Define& def);

	void operator()(If& if_stmt);

	void operator()(While& while_stmt);

	void operator()(For& for_stmt);

	void operator()(Break& break_stmt);

	void operator()(Continue& continue_stmt);

	void operator()(Return& return_stmt);

	void operator()(Struct& struct_stmt);

	void operator()(Binary& binary);

	void operator()(Group& group);

	void operator()(Unary& unary);

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

	void SetUpNativeFunctions();
};
