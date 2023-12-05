#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"
#include "Common/Value/Value.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/StaticData.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Interpreter/GarbageCollector/GarbageCollector.h"

#include <array>
#include <functional>
#include <unordered_map>
#include <iostream>
#include <stdexcept>
#include <format>

// handle std library
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class InterpreterException : public std::runtime_error
{
public:
	explicit InterpreterException(const std::string& message) : std::runtime_error(message) {}
};

class VirtualMachine
{

public:

	VirtualMachine(MidoriResult::ExecutableModule&& executable_module) : m_executable_module(std::move(executable_module)), m_garbage_collector(std::move(m_executable_module.m_constant_roots)),
		m_current_bytecode(&m_executable_module.m_modules[0]), m_instruction_pointer(m_current_bytecode->cbegin()) {}

	~VirtualMachine()
	{
		MidoriTraceable::CleanUp();
#ifdef _WIN32
		FreeLibrary(m_library_handle);
#else
		dlclose(m_library_handle);
#endif
	}

private:

	static constexpr int s_value_stack_max = 20000;
	static constexpr int s_frame_stack_max = 8000;
	static constexpr int s_garbage_collection_threshold = 1024;

	template <typename T, int Size>
	using StackPointer = std::array<T, Size>::iterator;
	using InstructionPointer = BytecodeStream::const_iterator;
	using GlobalVariables = std::unordered_map<std::string, MidoriValue>;

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
	std::vector<MidoriTraceable::Closure::Environment*> m_closure_stack;
	BytecodeStream* m_current_bytecode;
	std::unique_ptr<std::array<MidoriValue, s_value_stack_max>> m_value_stack = std::make_unique<std::array<MidoriValue, s_value_stack_max>>();
	std::unique_ptr<std::array<CallFrame, s_frame_stack_max>> m_call_stack = std::make_unique<std::array<CallFrame, s_frame_stack_max>>();
	StackPointer<MidoriValue, s_value_stack_max> m_base_pointer = m_value_stack->begin();
	StackPointer<MidoriValue, s_value_stack_max> m_value_stack_pointer = m_value_stack->begin();
	StackPointer<MidoriValue, s_value_stack_max> m_value_stack_end = m_value_stack->end();
	StackPointer<CallFrame, s_frame_stack_max> m_call_stack_pointer = m_call_stack->begin();
	StackPointer<CallFrame, s_frame_stack_max> m_call_stack_begin = m_call_stack->begin();
	StackPointer<CallFrame, s_frame_stack_max> m_call_stack_end = m_call_stack->end();
	InstructionPointer m_instruction_pointer;

#ifdef _WIN32
	HMODULE m_library_handle;
#else
	void* m_library_handle;
#endif

public:

	void Execute();

private:

	void DisplayRuntimeError(std::string_view message) noexcept
	{
		std::cerr << "\033[31m" << message << "\033[0m" << std::endl;
	}

	int GetLine() noexcept { return static_cast<int>(m_instruction_pointer - m_current_bytecode->cbegin()); }

	OpCode ReadByte() noexcept
	{
		OpCode op_code = static_cast<OpCode>(*m_instruction_pointer);
		++m_instruction_pointer;
		return op_code;
	}

	int ReadShort() noexcept { return static_cast<int>(ReadByte()) | (static_cast<int>(ReadByte()) << 8); }

	int ReadThreeBytes() noexcept
	{
		int value = static_cast<int>(ReadByte()) |
			(static_cast<int>(ReadByte()) << 8) |
			(static_cast<int>(ReadByte()) << 16);
		return value;
	}

	const MidoriValue& ReadConstant(OpCode operand_length) noexcept
	{
		int index = 0;

		switch (operand_length)
		{
		case OpCode::CONSTANT:
		{
			index = static_cast<int>(ReadByte());
			break;
		}
		case OpCode::CONSTANT_LONG:
		{
			index = static_cast<int>(ReadByte()) |
				(static_cast<int>(ReadByte()) << 8);
			break;
		}
		case OpCode::CONSTANT_LONG_LONG:
		{
			index = static_cast<int>(ReadByte()) |
				(static_cast<int>(ReadByte()) << 8) |
				(static_cast<int>(ReadByte()) << 16);
			break;
		}
		default:
		{
			break; // unreachable
		}
		}

		return m_executable_module.m_static_data.GetConstant(index);
	}

	const std::string& ReadGlobalVariable() noexcept
	{
		int index = static_cast<int>(ReadByte());
		return m_executable_module.m_global_table.GetGlobalVariable(index);
	}

	std::string GenerateRuntimeError(std::string_view message, int line) noexcept
	{
		MidoriTraceable::CleanUp();
		return MidoriError::GenerateRuntimeError(message, line);
	}

	void Push(const MidoriValue& value)
	{
		if (m_value_stack_pointer == m_value_stack_end)
		{
			throw InterpreterException(GenerateRuntimeError("Value stack overflow.", GetLine()));
		}

		*m_value_stack_pointer = value;
		++m_value_stack_pointer;
	}

	const MidoriValue& Peek() const noexcept
	{
		const MidoriValue& value = *(std::prev(m_value_stack_pointer));
		return value;
	}

	MidoriValue& Pop() noexcept
	{
		MidoriValue& value = *(--m_value_stack_pointer);
		return value;
	}

	template<typename T>
		requires (std::is_same_v<T, MidoriValue::MidoriInteger> || std::is_same_v<T, MidoriValue::MidoriFraction>)
	void CheckZeroDivision(const T& value)
	{
		if constexpr (std::is_same_v<T, MidoriValue::MidoriInteger>)
		{
			if (value == 0ll)
			{
				throw InterpreterException(GenerateRuntimeError("Division by zero.", GetLine()));
			}
		}
		else
		{
			if (value == 0.0)
			{
				throw InterpreterException(GenerateRuntimeError("Division by zero.", GetLine()));
			}
		}
	}

	void CheckIndexBounds(const MidoriValue& index, int64_t size)
	{
		MidoriValue::MidoriInteger val = index.GetInteger();
		if (val < 0ll || val >= size)
		{
			throw InterpreterException(GenerateRuntimeError(std::format("Index out of bounds at index: {}.", val), GetLine()));
		}
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
	MidoriTraceable* AllocateObject(T&& value) { return MidoriTraceable::AllocateObject(std::forward<T>(value)); }
};
