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

	using ValueStackPointer = std::array<MidoriValue, s_value_stack_max>::iterator;
	using InstructionPointer = const OpCode*;
	using GlobalVariables = std::unordered_map<std::string, MidoriValue>;

	struct CallFrame
	{
		ValueStackPointer  m_return_bp;
		ValueStackPointer  m_return_sp;
		InstructionPointer m_return_ip = nullptr;
	};

	using CallStackPointer = std::array<CallFrame, s_frame_stack_max>::iterator;

	MidoriExecutable m_executable;
	GlobalVariables m_global_vars;
	GarbageCollector m_garbage_collector;
	std::vector<MidoriTraceable::Closure::Environment*> m_closure_stack;
	std::unique_ptr<std::array<MidoriValue, s_value_stack_max>> m_value_stack = std::make_unique<std::array<MidoriValue, s_value_stack_max>>();
	std::unique_ptr<std::array<CallFrame, s_frame_stack_max>> m_call_stack = std::make_unique<std::array<CallFrame, s_frame_stack_max>>();
	ValueStackPointer m_base_pointer = m_value_stack->begin();
	ValueStackPointer m_value_stack_pointer = m_value_stack->begin();
	ValueStackPointer m_value_stack_end = m_value_stack->end();
	CallStackPointer m_call_stack_pointer = m_call_stack->begin();
	CallStackPointer m_call_stack_begin = m_call_stack->begin();
	CallStackPointer m_call_stack_end = m_call_stack->end();
	InstructionPointer m_instruction_pointer;

#ifdef _WIN32
	HMODULE m_library_handle = nullptr;
#else
	void* m_library_handle = nullptr;
#endif

public:

	void Execute();

private:

	void DisplayRuntimeError(std::string_view message) noexcept;

	// only used for error reporting, efficiency is not a concern
	int GetLine();

	OpCode ReadByte() noexcept;

	int ReadShort() noexcept;

	int ReadThreeBytes() noexcept;

	const MidoriValue& ReadConstant(OpCode operand_length) noexcept;

	const std::string& ReadGlobalVariable() noexcept;

	std::string GenerateRuntimeError(std::string_view message, int line) noexcept;

	void PushCallFrame(ValueStackPointer m_return_bp, ValueStackPointer m_return_sp, InstructionPointer m_return_ip);

	const MidoriValue& Peek() const noexcept;

	MidoriValue& Pop() noexcept;

	void CheckIndexBounds(const MidoriValue& index, MidoriValue::MidoriInteger size);

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

	template<typename... Args>
	void Push(Args&&... args)
	{
		if (m_value_stack_pointer != m_value_stack_end)
		{
			*m_value_stack_pointer = MidoriValue(std::forward<Args>(args)...);
			++m_value_stack_pointer;
			return;
		}
		
		throw InterpreterException(GenerateRuntimeError("Value stack overflow.", GetLine()));
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
};
