#pragma once

#include "Compiler/AbstractSyntaxTree/AbstractSyntaxTree.h"
#include "Compiler/Error/Error.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/StaticData.h"
#include "Common/Value/GlobalVariableTable.h"

#include <unordered_map>

class CodeGenerator
{
public:
	struct CodeGeneratorResult
	{
		BytecodeStream m_module;
		Traceable::GarbageCollectionRoots m_constant_roots;
		StaticData m_static_data;
		GlobalVariableTable m_global_table;
#ifdef DEBUG
		std::vector<const Object::DefinedFunction*> m_sub_modules;
#endif
		bool m_error;
		
#ifdef DEBUG
		CodeGeneratorResult(BytecodeStream&& bytecode, Traceable::GarbageCollectionRoots&& roots, std::vector<const Object::DefinedFunction*>&& modules, StaticData&& static_data, GlobalVariableTable&& global_table, bool error) : m_module(std::move(bytecode)), m_constant_roots(std::move(roots)), m_sub_modules(std::move(modules)), m_static_data(std::move(static_data)), m_global_table(std::move(global_table)), m_error(error) {}
#else
		CodeGeneratorResult(BytecodeStream&& bytecode, Traceable::GarbageCollectionRoots&& roots, StaticData&& static_data, GlobalVariableTable&& global_table, bool error) : m_module(std::move(bytecode)), m_constant_roots(std::move(roots)), m_static_data(std::move(static_data)), m_global_table(std::move(global_table)), m_error(error) {}
#endif
	};

private:
	BytecodeStream m_module;
	std::unordered_map<std::string, int> m_global_variables;
#ifdef DEBUG
	std::vector<const Object::DefinedFunction*> m_sub_modules;
#endif
	StaticData m_static_data;
	GlobalVariableTable m_global_table;
	Traceable::GarbageCollectionRoots m_traceable_constants;
	bool m_error = false;

public:

	CodeGeneratorResult GenerateCode(ProgramTree&& program_tree);

private:

	inline void EmitByte(OpCode byte, int line) { m_module.AddByteCode(byte, line); }

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

	inline void EmitConstant(Value&& value, int line)
	{
		if (value.IsObjectPointer())
		{
			m_traceable_constants.emplace(value.GetObjectPointer());
		}

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
			CompilerError::PrintError("Too many constants (max 16777215).", line);
			m_error = true;
		}
	}

	inline void EmitVariable(int variable_index, OpCode op, int line)
	{
		if (variable_index > UINT8_MAX)
		{
			CompilerError::PrintError("Too many variables (max 255).", line);
			m_error = true;
			return;
		}

		EmitByte(op, line);
		EmitByte(static_cast<OpCode>(variable_index), line);
	}

	inline int EmitJump(OpCode op, int line)
	{
		EmitByte(op, line);
		EmitByte(static_cast<OpCode>(0xff), line);
		EmitByte(static_cast<OpCode>(0xff), line);
		return m_module.GetByteCodeSize() - 2;
	}

	inline void PatchJump(int offset, int line)
	{
		int jump = m_module.GetByteCodeSize() - offset - 2;
		if (jump > UINT16_MAX)
		{
			CompilerError::PrintError("Too much code to jump over (max 65535).", line);
			m_error = true;
		}

		m_module.SetByteCode(offset, static_cast<OpCode>(jump & 0xff));
		m_module.SetByteCode(offset + 1, static_cast<OpCode>((jump >> 8) & 0xff));
	}

	inline void EmitLoop(int loop_start, int line) 
	{
		EmitByte(OpCode::JUMP_BACK, line);

		int offset = m_module.GetByteCodeSize() - loop_start + 2;
		if (offset > UINT16_MAX)
		{ 
			CompilerError::PrintError("Loop body too large (max 65535).", line);
			m_error = true;
			return;
		}

		EmitByte(static_cast<OpCode>(offset & 0xff), line);
		EmitByte(static_cast<OpCode>((offset >> 8) & 0xff), line);
	}

	void operator()(Block& block);

	void operator()(Simple& simple);

	void operator()(Let& let);

	void operator()(If& if_stmt);

	void operator()(While& while_stmt);

	void operator()(For& for_stmt);

	void operator()(Break&);

	void operator()(Continue&);

	void operator()(Return& return_stmt);

	void operator()(Import& import);

	void operator()(Namespace& namespace_stmt);

	void operator()(Halt&);

	void operator()(Binary& binary);

	void operator()(Pipe& pipe);

	void operator()(Logical& logical);

	void operator()(Group& group);

	void operator()(Unary& unary);

	void operator()(Call& call);

	void operator()(Get& get);

	void operator()(Set& set);

	void operator()(Variable& variable);

	void operator()(Assign& assign);

	void operator()(String& string);

	void operator()(Bool& bool_expr);

	void operator()(Number& number);

	void operator()(Nil& nil);

	void operator()(Function& function);

	void operator()(Array& array);

	void operator()(ArrayGet& array_get);

	void operator()(ArraySet& array_set);

	void operator()(Ternary& ternary);

	void SetUpGlobalVariables();
};