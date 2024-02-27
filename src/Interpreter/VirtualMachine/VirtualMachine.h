#pragma once

#include "Common/Error/Error.h"
#include "Common/Result/Result.h"
#include "Common/Value/Value.h"
#include "Common/Executable/Executable.h"
#include "Interpreter/GarbageCollector/GarbageCollector.h"

#include <array>
#include <functional>
#include <unordered_map>

// handle std library
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class VirtualMachine
{

public:

	VirtualMachine(MidoriExecutable&& executable) noexcept;

	~VirtualMachine();

private:

	static constexpr int s_value_stack_max = 30000;
	static constexpr int s_call_stack_max = 10000;
	static constexpr int s_garbage_collection_threshold = 1024;

	using ValueStackPointer = MidoriValue*;
	using InstructionPointer = const OpCode*;
	using GlobalVariables = std::unordered_map<std::string, MidoriValue>;

	struct CallFrame
	{
		ValueStackPointer  m_return_bp;
		ValueStackPointer  m_return_sp;
		InstructionPointer m_return_ip = nullptr;
	};

	using CallStackPointer = CallFrame*;

	MidoriExecutable m_executable;
	GlobalVariables m_global_vars;
	GarbageCollector m_garbage_collector;
	std::vector<MidoriTraceable::CellValue*> m_cells_to_promote;
	std::unique_ptr<std::array<MidoriTraceable::MidoriClosure::Environment*, s_call_stack_max>> m_closure_stack = std::make_unique<std::array<MidoriTraceable::MidoriClosure::Environment*, s_call_stack_max>>();
	std::unique_ptr<std::array<MidoriValue, s_value_stack_max>> m_value_stack = std::make_unique<std::array<MidoriValue, s_value_stack_max>>();
	std::unique_ptr<std::array<CallFrame, s_call_stack_max>> m_call_stack = std::make_unique<std::array<CallFrame, s_call_stack_max>>();
	ValueStackPointer m_base_pointer = &(*m_value_stack)[0u];
	ValueStackPointer m_value_stack_pointer = &(*m_value_stack)[0u];
	ValueStackPointer m_value_stack_begin = &(*m_value_stack)[0u];
	ValueStackPointer m_value_stack_end = &(*m_value_stack)[static_cast<size_t>(s_value_stack_max) - 1u];
	CallStackPointer m_call_stack_pointer = &(*m_call_stack)[0u];
	CallStackPointer m_call_stack_begin = &(*m_call_stack)[0u];
	CallStackPointer m_call_stack_end = &(*m_call_stack)[static_cast<size_t>(s_call_stack_max) - 1u];
	MidoriTraceable::MidoriClosure::Environment** m_env_pointer = &(*m_closure_stack)[0u];
	MidoriTraceable::MidoriClosure::Environment** m_env_pointer_begin = &(*m_closure_stack)[0u];
	MidoriTraceable::MidoriClosure::Environment** m_env_pointer_end = &(*m_closure_stack)[static_cast<size_t>(s_call_stack_max) - 1u];
	InstructionPointer m_instruction_pointer;

#ifdef _WIN32
	HMODULE m_library_handle = nullptr;
#else
	void* m_library_handle = nullptr;
#endif

public:

	void Execute() noexcept;

private:

	void TerminateExecution(std::string_view message) noexcept;

	// only used for error reporting, efficiency is not a concern
	int GetLine() noexcept;

	OpCode ReadByte() noexcept;

	int ReadShort() noexcept;

	int ReadThreeBytes() noexcept;

	const MidoriValue& ReadConstant(OpCode operand_length) noexcept;

	const std::string& ReadGlobalVariable() noexcept;

	std::string GenerateRuntimeError(std::string_view message, int line) noexcept;

	void PushCallFrame(ValueStackPointer m_return_bp, ValueStackPointer m_return_sp, InstructionPointer m_return_ip) noexcept;

	MidoriValue& Peek() noexcept;

	MidoriValue& Pop() noexcept;

	void PromoteCells() noexcept;

	void CheckIndexBounds(const MidoriValue& index, MidoriValue::MidoriInteger size) noexcept;

	MidoriTraceable::GarbageCollectionRoots GetValueStackGarbageCollectionRoots() const noexcept;

	MidoriTraceable::GarbageCollectionRoots GetGlobalTableGarbageCollectionRoots() const noexcept;

	MidoriTraceable::GarbageCollectionRoots GetGarbageCollectionRoots() const noexcept;

	void CollectGarbage() noexcept;

	template<typename T>
	MidoriTraceable* CollectGarbageThenAllocateTraceable(T&& arg) noexcept
	{
		CollectGarbage();
		return MidoriTraceable::AllocateTraceable(std::forward<T>(arg));
	}

	template<typename... Args>
		requires MidoriValueConstructible<Args...>
	void Push(Args&&... args) noexcept
	{
		if (m_value_stack_pointer <= m_value_stack_end) [[likely]]
		{
			std::construct_at(m_value_stack_pointer, std::forward<Args>(args)...);
			++m_value_stack_pointer;
			return;
		}
		else [[unlikely]]
		{
			TerminateExecution(GenerateRuntimeError("Value stack overflow.", GetLine()));
		}
	}

	template<typename T>
		requires MidoriNumeric<T>
	void CheckZeroDivision(const T& value) noexcept
	{
		if constexpr (std::is_same_v<T, MidoriValue::MidoriInteger>)
		{
			if (value == 0ll) [[unlikely]]
			{
				TerminateExecution(GenerateRuntimeError("Division by zero.", GetLine()));
			}
		}
		else
		{
			if (value == 0.0) [[unlikely]]
			{
				TerminateExecution(GenerateRuntimeError("Division by zero.", GetLine()));
			}
		}
	}
};
