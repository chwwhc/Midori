#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"
#include "Common/Value/Value.h"
#include "Common/Executable/Executable.h"
#include "Interpreter/GarbageCollector/GarbageCollector.h"

#include <array>
#include <functional>
#include <unordered_map>
#include <iostream>
#include <stdexcept>
#include <format>
#include <string_view>

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

	VirtualMachine(MidoriExecutable&& executable) : m_executable(std::move(executable)), m_garbage_collector(m_executable.GetConstantRoots())
	{
		constexpr int runtime_startup_proc_index = 0;
		m_instruction_pointer = &*m_executable.GetBytecodeStream(runtime_startup_proc_index).cbegin();
	}

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

	template <typename T>
	using StackPointer = T*;
	using InstructionPointer = const OpCode*;
	using GlobalVariables = std::unordered_map<std::string, MidoriValue>;

	struct CallFrame
	{
		StackPointer<MidoriValue>  m_return_bp = nullptr;
		StackPointer<MidoriValue>  m_return_sp = nullptr;
		InstructionPointer m_return_ip = nullptr;
	};

	MidoriExecutable m_executable;
	GlobalVariables m_global_vars;
	GarbageCollector m_garbage_collector;
	std::vector<MidoriTraceable::Closure::Environment*> m_closure_stack;
	std::unique_ptr<std::array<MidoriValue, s_value_stack_max>> m_value_stack = std::make_unique<std::array<MidoriValue, s_value_stack_max>>();
	std::unique_ptr<std::array<CallFrame, s_frame_stack_max>> m_call_stack = std::make_unique<std::array<CallFrame, s_frame_stack_max>>();
	StackPointer<MidoriValue> m_base_pointer = &*m_value_stack->begin();
	StackPointer<MidoriValue> m_value_stack_pointer = &*m_value_stack->begin();
	StackPointer<MidoriValue> m_value_stack_end = &*std::prev(m_value_stack->end());
	StackPointer<CallFrame> m_call_stack_pointer = &*m_call_stack->begin();
	StackPointer<CallFrame> m_call_stack_begin = &*m_call_stack->begin();
	StackPointer<CallFrame> m_call_stack_end = &*std::prev(m_call_stack->end());
	InstructionPointer m_instruction_pointer;

#ifdef _WIN32
	HMODULE m_library_handle = nullptr;
#else
	void* m_library_handle = nullptr;
#endif

public:

	void Execute();

private:

	void DisplayRuntimeError(std::string_view message) noexcept
	{
		std::cerr << "\033[31m" << message << "\033[0m" << std::endl;
	}

	// only used for error reporting, efficiency is not a concern
	int GetLine()
	{
		for (int i = 0; i < m_executable.GetProcedureCount(); i += 1)
		{
			if (m_instruction_pointer >= &*m_executable.GetBytecodeStream(i).cbegin() && m_instruction_pointer <= &*std::prev(m_executable.GetBytecodeStream(i).cend()))
			{
				return m_executable.GetLine(static_cast<int>(m_instruction_pointer - &*m_executable.GetBytecodeStream(i).cbegin()), i);
			}
		}

		throw InterpreterException(GenerateRuntimeError("Invalid instruction pointer.", 0));
	}

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

		return m_executable.GetConstant(index);
	}

	const std::string& ReadGlobalVariable() noexcept
	{
		int index = static_cast<int>(ReadByte());
		return m_executable.GetGlobalVariable(index);
	}

	std::string GenerateRuntimeError(std::string_view message, int line) noexcept
	{
		MidoriTraceable::CleanUp();
		return MidoriError::GenerateRuntimeError(message, line);
	}

	template<typename... Args>
	void Push(Args&&... args)
	{
		if (m_value_stack_pointer == m_value_stack_end + 1)
		{
			throw InterpreterException(GenerateRuntimeError("Value stack overflow.", GetLine()));
		}

		*m_value_stack_pointer = MidoriValue(std::forward<Args>(args)...);
		++m_value_stack_pointer;
	}

	void PushCallFrame(StackPointer<MidoriValue> m_return_bp, StackPointer<MidoriValue> m_return_sp, InstructionPointer m_return_ip)
	{
		if (m_call_stack_pointer == m_call_stack_end + 1)
		{
			throw InterpreterException(GenerateRuntimeError("Call stack overflow.", GetLine()));
		}

		*m_call_stack_pointer = { m_return_bp, m_return_sp, m_return_ip };
		++m_call_stack_pointer;
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
