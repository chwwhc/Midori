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
#include <stack>

#define VALUE_STACK_MAX 7200
#define FRAME_STACK_MAX 2050
#define GARBAGE_COLLECTION_THRESHOLD 1024

class VirtualMachine
{

public:

	VirtualMachine(MidoriResult::ExecutableModule&& module) : m_executable_module(std::move(module)), m_garbage_collector(std::move(m_executable_module.m_constant_roots)), m_current_bytecode(&m_executable_module.m_modules[0]), m_instruction_pointer(m_current_bytecode->cbegin()) {}

	~VirtualMachine() { Traceable::CleanUp(); }

private:
	template <typename T, int Size>
	using StackPointer = std::array<T, Size>::iterator;
	using InstructionPointer = BytecodeStream::const_iterator;
	using GlobalVariables = std::unordered_map<std::string, MidoriValue>;
	friend class NativeFunction;

	struct CallFrame
	{
		BytecodeStream* m_return_module;
		StackPointer<MidoriValue, VALUE_STACK_MAX>  m_return_bp;
		StackPointer<MidoriValue, VALUE_STACK_MAX>  m_return_sp;
		InstructionPointer m_return_ip;
	};

	std::array<MidoriValue, VALUE_STACK_MAX> m_value_stack;
	std::array<CallFrame, FRAME_STACK_MAX> m_call_stack;
	MidoriResult::ExecutableModule m_executable_module;
	GlobalVariables m_global_vars;
	GarbageCollector m_garbage_collector;
	std::stack<Traceable::Closure*> m_closure_stack;
	BytecodeStream* m_current_bytecode;
	StackPointer<MidoriValue, VALUE_STACK_MAX> m_base_pointer = m_value_stack.begin();
	StackPointer<MidoriValue, VALUE_STACK_MAX> m_value_stack_pointer = m_value_stack.begin();
	StackPointer<CallFrame, FRAME_STACK_MAX> m_call_stack_pointer = m_call_stack.begin();
	InstructionPointer m_instruction_pointer;

public:
	MidoriResult::InterpreterResult Execute();

private:
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
		Traceable::CleanUp();
		return MidoriError::GenerateRuntimeError(message.data(), line);
	}

	inline MidoriResult::InterpreterResult Push(MidoriValue&& value)
	{
		if (m_value_stack_pointer == m_value_stack.end())
		{
			return std::unexpected<std::string>(GenerateRuntimeError("Value stack overflow.", GetLine()));
		}

		*m_value_stack_pointer = std::move(value);
		++m_value_stack_pointer;
		return &(*std::prev(m_value_stack_pointer));
	}

	inline MidoriResult::InterpreterResult Duplicate()
	{
		if (m_value_stack_pointer == m_value_stack.end())
		{
			return std::unexpected<std::string>(GenerateRuntimeError("Value stack overflow.", GetLine()));
		}

		MidoriValue top_value = *std::prev(m_value_stack_pointer);
		*m_value_stack_pointer = std::move(top_value);
		++m_value_stack_pointer;
		return &(*std::prev(m_value_stack_pointer));
	}

	inline MidoriResult::InterpreterResult Pop() noexcept
	{
		MidoriValue& value = *(--m_value_stack_pointer);
		if (value.IsObjectPointer() && value.GetObjectPointer()->IsCellValue())
		{
			return &value.GetObjectPointer()->GetCellValue();
		}
		else
		{
			return &value;
		}
	}

	inline MidoriResult::InterpreterResult CheckIsArray(MidoriValue* val)
	{
		if (!val->IsObjectPointer() || !val->GetObjectPointer()->IsArray())
		{
			return std::unexpected<std::string>(GenerateRuntimeError("Operand must be an array.", GetLine()));
		}
		else
		{
			return val;
		}
	}

	inline MidoriResult::InterpreterResult CheckIsNumber(MidoriValue* val)
	{
		if (!val->IsNumber())
		{
			return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a number.", GetLine()));
		}
		else
		{
			return val;
		}
	}

	inline MidoriResult::InterpreterResult CheckIsInteger(MidoriValue* num)
	{
		double val = num->GetNumber();
		if (val != static_cast<int>(val))
		{
			return std::unexpected<std::string>(GenerateRuntimeError("Index must be an integer.", GetLine()));
		}
		else
		{
			return num;
		}
	}

	inline MidoriResult::InterpreterResult CheckIndexBounds(MidoriValue* index, int size)
	{
		double val = index->GetNumber();
		int val_int = static_cast<int>(val);
		if (val_int < 0 || val_int >= size)
		{
			std::string error_message = "Index out of bounds at index: " + std::to_string(val_int) + ".";
			return std::unexpected<std::string>(GenerateRuntimeError(error_message, GetLine()));
		}
		else
		{
			return index;
		}
	}

	inline MidoriResult::InterpreterResult CheckArrayMaxLength(MidoriValue* num)
	{
		double val = num->GetNumber();
		if (val < 0 || val > THREE_BYTE_MAX)
		{
			std::string error_message = "Length must be between 0 and " + std::to_string(THREE_BYTE_MAX) + ".";
			return std::unexpected<std::string>(GenerateRuntimeError(error_message, GetLine()));
		}
		else
		{
			return num;
		}
	}

	inline MidoriResult::InterpreterResult Bind(MidoriResult::InterpreterResult&& prev, std::function<MidoriResult::InterpreterResult(MidoriValue*)>&& function) { return !prev.has_value() ? std::move(prev) : function(prev.value()); }

	inline static bool AreSameType(const MidoriValue& left, const MidoriValue& right) { return MidoriValue::AreSameType(left, right); }

	inline static bool AreNumerical(const MidoriValue& left, const MidoriValue& right) { return left.IsNumber() && right.IsNumber(); }

	inline static bool AreConcatenatable(const MidoriValue& left, const MidoriValue& right)
	{
		if (!left.IsObjectPointer() || !right.IsObjectPointer())
		{
			return false;
		}

		Traceable* left_value = left.GetObjectPointer();
		Traceable* right_value = right.GetObjectPointer();

		return (left_value->IsString() && right_value->IsString()) || (left_value->IsArray() && right_value->IsArray());
	}

	inline static bool Are32BitIntegers(const MidoriValue& left, const MidoriValue& right)
	{
		if (!AreNumerical(left, right))
		{
			return false;
		}

		double left_value = left.GetNumber();
		double right_value = right.GetNumber();

		return left_value >= INT_MIN && left_value <= INT_MAX &&
			right_value >= INT_MIN && right_value <= INT_MAX;
	}

	Traceable::GarbageCollectionRoots GetValueStackGarbageCollectionRoots();

	Traceable::GarbageCollectionRoots GetGlobalTableGarbageCollectionRoots();

	Traceable::GarbageCollectionRoots GetGarbageCollectionRoots();

	void CollectGarbage();

	template<typename T>
	Traceable* CollectGarbageThenAllocateObject(T&& value)
	{
		CollectGarbage();
		return Traceable::AllocateObject(std::forward<T>(value));
	}

	template<typename T>
	Traceable* AllocateObject(T&& value)
	{
		return Traceable::AllocateObject(std::forward<T>(value));
	}

	MidoriResult::InterpreterResult BinaryOperation(std::function<MidoriValue(const MidoriValue&, const MidoriValue&)>&& op, bool (*type_checker)(const MidoriValue&, const MidoriValue&));
};
