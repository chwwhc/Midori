#pragma once

#include "Common/Value/Value.h"
#include "Common/BytecodeStream/BytecodeStream.h"
#include "Common/Value/StaticData.h"
#include "Common/Value/GlobalVariableTable.h"
#include "Interpreter/GarbageCollector/GarbageCollector.h"
#include "Interpreter/NativeFunction/NativeFunction.h"

#include <array>
#include <functional>
#include <unordered_map>
#include <utility>
#include <iostream>

#define VALUE_STACK_MAX 512
#define FRAME_STACK_MAX 256
#define GARBAGE_COLLECTION_THRESHOLD 1024

class VirtualMachine
{

public:

	VirtualMachine(BytecodeStream&& bytecode, Traceable::GarbageCollectionRoots&& constant_roots, StaticData&& static_data, GlobalVariableTable&& global_table) : m_module(std::move(bytecode)), m_garbage_collector(GarbageCollector(std::move(constant_roots))), m_static_data(std::move(static_data)), m_global_table(std::move(global_table)), m_current_bytecode(&m_module), m_instruction_pointer(m_current_bytecode->cbegin()) {}

	~VirtualMachine() { Traceable::CleanUp(); }

private:
	template <typename T, int Size>
	using StackPointer = std::array<T, Size>::iterator;
	using InstructionPointer = BytecodeStream::const_iterator;
	using GlobalVariables = std::unordered_map<std::string, Value>;
	friend class NativeFunction;

	struct CallFrame
	{
		BytecodeStream* m_return_module = nullptr;
		StackPointer<Value, VALUE_STACK_MAX>  m_return_bp;
		StackPointer<Value, VALUE_STACK_MAX>  m_return_sp;
		InstructionPointer m_return_ip;
	};

	std::array<Value, VALUE_STACK_MAX> m_value_stack;
	std::array<CallFrame, FRAME_STACK_MAX> m_call_stack;
	BytecodeStream m_module;
	StaticData m_static_data;
	GlobalVariables m_global_vars;
	GlobalVariableTable m_global_table;
	GarbageCollector m_garbage_collector;
	Object::Closure* m_current_closure = nullptr;
	BytecodeStream* m_current_bytecode = nullptr;
	StackPointer<Value, VALUE_STACK_MAX> m_base_pointer = m_value_stack.begin();
	StackPointer<Value, VALUE_STACK_MAX> m_value_stack_pointer = m_value_stack.begin();
	StackPointer<CallFrame, FRAME_STACK_MAX> m_call_stack_pointer = m_call_stack.begin();
	InstructionPointer m_instruction_pointer;
	bool m_error = false;

public:
	void Execute();

private:
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

	inline const Value& ReadConstant(OpCode operand_length)
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

		return m_static_data.GetConstant(index);
	}
	
	inline const std::string& ReadGlobalVariable()
	{
		int index = static_cast<int>(ReadByte());
		return m_global_table.GetGlobalVariable(index);
	}

	inline void Push(Value&& value)
	{
		{
			if (m_value_stack_pointer == m_value_stack.end())
			{
				std::cerr << "Value stack overflow." << std::endl;
				m_error = true;
			}

			*m_value_stack_pointer++ = std::move(value);
		}
	}

	inline Value Pop()
	{
		if (m_value_stack_pointer == m_base_pointer)
		{
			std::cerr << "Value stack underflow." << std::endl;
			m_error = true;
		}
		return std::move(*(--m_value_stack_pointer));
	}

	inline const Value& Peek(int distance)
	{
		if (m_value_stack_pointer - 1 - distance < m_base_pointer)
		{
			std::cerr << "Value stack underflow." << std::endl;
			m_error = true;
		}
		return *(m_value_stack_pointer - 1 - distance);
	}

	inline void CheckIsArray(const Value& val)
	{
		if (!val.IsObjectPointer() || !val.GetObjectPointer()->IsArray())
		{
			std::cerr << "Operand must be an array." << std::endl;
			m_error = true;
		}
	}

	inline void CheckIsNumber(const Value& val)
	{
		if (!val.IsNumber())
		{
			std::cerr << "Index must be a number." << std::endl;
			m_error = true;
		}
	}

	inline void CheckIsInteger(double num)
	{
		if (num != static_cast<int>(num))
		{
			std::cerr << "Index must be an integer." << std::endl;
			m_error = true;
		}
	}

	inline void CheckIndexBounds(int index, int size)
	{
		if (index < 0 || index >= size)
		{
			std::cerr << "Index out of bounds at index: " << index << std::endl;
			m_error = true;
		}
	}

	inline void CheckArrayMaxLength(double val)
	{
		if (val < 0 || val > THREE_BYTE_MAX)
		{
			std::cerr << "Length must be between 0 and " << THREE_BYTE_MAX << "." << std::endl;
			m_error = true;
		}
	}

	inline static bool AreSameType(const Value& left, const Value& right) { return Value::AreSameType(left, right); }

	inline static bool AreNumerical(const Value& left, const Value& right){ return left.IsNumber() && right.IsNumber(); }

	inline static bool AreConcatenatable(const Value& left, const Value& right) 
	{ 
		if (!left.IsObjectPointer() || !right.IsObjectPointer())
		{
			return false;
		}

		Object* left_value = left.GetObjectPointer();
		Object* right_value = right.GetObjectPointer();

		return (left_value->IsString() && right_value->IsString()) || (left_value->IsArray() && right_value->IsArray());
	}

	inline static bool Are32BitIntegers(const Value& left, const Value &right) 
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

	Traceable::GarbageCollectionRoots GetGlobalGarbageCollectionRoots();

	Traceable::GarbageCollectionRoots GetGarbageCollectionRoots();

	void CollectGarbage();

	template<typename T>
	Object* RuntimeAllocateObject(T&& value)
	{
		CollectGarbage();
		return Object::AllocateObject(std::forward<T>(value));
	}

	void BinaryOperation(std::function<Value(const Value&, const Value&)>&& op, bool (*type_checker)(const Value&, const Value&));
};
