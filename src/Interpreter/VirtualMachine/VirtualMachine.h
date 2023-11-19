#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"
#include "Common/Value/Value.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/StaticData.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Interpreter/GarbageCollector/GarbageCollector.h"
#include "Interpreter/NativeFunction/NativeFunction.h"

#include <array>
#include <functional>
#include <unordered_map>
#include <iostream>
#include <stack>
#include <format>

class VirtualMachine
{

public:

	VirtualMachine(MidoriResult::ExecutableModule&& module) : m_executable_module(std::move(module)), m_garbage_collector(std::move(m_executable_module.m_constant_roots)), m_current_bytecode(&m_executable_module.m_modules[0]), m_instruction_pointer(m_current_bytecode->cbegin()) {}

	~VirtualMachine() { MidoriTraceable::CleanUp(); }

private:

	static constexpr int s_value_stack_max = 20000;
static constexpr int s_frame_stack_max = 8000;
static constexpr int s_garbage_collection_threshold = 1024;

	template <typename T, int Size>
	using StackPointer = std::array<T, Size>::iterator;
	using InstructionPointer = BytecodeStream::const_iterator;
	using GlobalVariables = std::unordered_map<std::string, MidoriValue>;
	friend class NativeFunction;

	struct CallFrame
	{
		BytecodeStream* m_return_module;
		StackPointer<MidoriValue, s_value_stack_max>  m_return_bp;
		StackPointer<MidoriValue, s_value_stack_max>  m_return_sp;
		InstructionPointer m_return_ip;
	};

	MidoriResult::ExecutableModule m_executable_module;
	GlobalVariables m_global_vars;
	GarbageCollector m_garbage_collector;
	std::stack<MidoriTraceable::Closure*> m_closure_stack;
	BytecodeStream* m_current_bytecode;
	std::unique_ptr<std::array<MidoriValue, s_value_stack_max>> m_value_stack = std::make_unique<std::array<MidoriValue, s_value_stack_max>>();
	std::unique_ptr<std::array<CallFrame, s_frame_stack_max>> m_call_stack = std::make_unique<std::array<CallFrame, s_frame_stack_max>>();
	StackPointer<MidoriValue, s_value_stack_max> m_base_pointer = m_value_stack->begin();
	StackPointer<MidoriValue, s_value_stack_max> m_value_stack_pointer = m_value_stack->begin();
	StackPointer<CallFrame, s_frame_stack_max> m_call_stack_pointer = m_call_stack->begin();
	InstructionPointer m_instruction_pointer;
	bool m_error = false;

public:
	void Execute();

private:

	inline void DisplayRuntimeError(std::string_view message)
	{
		std::cerr << "\033[31m" << message << "\033[0m" << std::endl;
		m_error = true;
	}

	inline int GetLine() { return static_cast<int>(m_instruction_pointer - m_current_bytecode->cbegin()); }

	inline OpCode ReadByte()
	{
		return static_cast<OpCode>(*(m_instruction_pointer++));
	}

	inline int ReadShort()
	{
		int value = static_cast<int>(ReadByte()) | (static_cast<int>(ReadByte()) << 8);
		return value;
	}

	inline int ReadThreeBytes()
	{
		int value = static_cast<int>(ReadByte()) |
			(static_cast<int>(ReadByte()) << 8) |
			(static_cast<int>(ReadByte()) << 16);
		return value;
	}

	inline MidoriValue ReadConstant(OpCode operand_length)
	{
		int index = 0;

		if (operand_length == OpCode::CONSTANT)
		{
			index = static_cast<int>(ReadByte());
		}
		else if (operand_length == OpCode::CONSTANT_LONG)
		{
			index = static_cast<int>(ReadByte()) |
				(static_cast<int>(ReadByte()) << 8);
		}
		else if (operand_length == OpCode::CONSTANT_LONG_LONG)
		{
			index = static_cast<int>(ReadByte()) |
				(static_cast<int>(ReadByte()) << 8) |
				(static_cast<int>(ReadByte()) << 16);
		}

		return m_executable_module.m_static_data.GetConstant(index);
	}

	inline const std::string& ReadGlobalVariable()
	{
		int index = static_cast<int>(ReadByte());
		return m_executable_module.m_global_table.GetGlobalVariable(index);
	}

	inline std::string GenerateRuntimeError(const std::string& message, int line)
	{
		MidoriTraceable::CleanUp();
		return MidoriError::GenerateRuntimeError(message.data(), line);
	}

	inline bool Push(MidoriValue&& value)
	{
		if (m_value_stack_pointer == m_value_stack->end())
		{
			DisplayRuntimeError(GenerateRuntimeError("Value stack overflow.", GetLine()));
			return false;
		}

		*m_value_stack_pointer = std::move(value);
		++m_value_stack_pointer;
		return true;
	}

	inline bool Duplicate()
	{
		if (m_value_stack_pointer == m_value_stack->end())
		{
			DisplayRuntimeError(GenerateRuntimeError("Value stack overflow.", GetLine()));
			return false;
		}

		MidoriValue top_value = *std::prev(m_value_stack_pointer);
		*m_value_stack_pointer = std::move(top_value);
		++m_value_stack_pointer;
		return true;
	}

	inline MidoriValue& Pop() noexcept
	{
		MidoriValue& value = *(--m_value_stack_pointer);
		if (value.IsObjectPointer() && value.GetObjectPointer()->IsCellValue())
		{
			return value.GetObjectPointer()->GetCellValue();
		}
		else
		{
			return value;
		}
	}

	inline bool CheckIndexBounds(MidoriValue* index, int64_t size)
	{
		MidoriValue::MidoriInteger val = index->GetInteger();
		if (val < 0ll || val >= size)
		{
			DisplayRuntimeError(GenerateRuntimeError(std::format("Index out of bounds at index: {}.", val), GetLine()));
			return false;
		}

		return true;
	}

	MidoriTraceable::GarbageCollectionRoots GetValueStackGarbageCollectionRoots();

	MidoriTraceable::GarbageCollectionRoots GetGlobalTableGarbageCollectionRoots();

	MidoriTraceable::GarbageCollectionRoots GetGarbageCollectionRoots();

	void CollectGarbage();

	template<typename T>
	MidoriTraceable* CollectGarbageThenAllocateObject(T&& value)
	{
		CollectGarbage();
		return MidoriTraceable::AllocateObject(std::forward<T>(value));
	}

	template<typename T>
	MidoriTraceable* AllocateObject(T&& value)
	{
		return MidoriTraceable::AllocateObject(std::forward<T>(value));
	}
};
