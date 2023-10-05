#pragma once

#include "Common/Value/Value.h"
#include "Common/ExecutableModule/ExecutableModule.h"

#include <array>
#include <functional>
#include <unordered_map>
#include <iostream>

#define VALUE_STACK_MAX 512
#define FRAME_STACK_MAX 256

class VirtualMachine
{

public:
	VirtualMachine(ExecutableModule&& module) : m_module(std::move(module)), m_current_module(&m_module), m_instruction_pointer(m_current_module->cbegin()) {}

private:
	template <typename T, int Size>
	using StackPointer = std::array<T, Size>::iterator;
	using InstructionPointer = ExecutableModule::const_iterator;

	struct CallFrame
	{
		ExecutableModule* m_return_module = nullptr;
		StackPointer<Value, VALUE_STACK_MAX>  m_return_bp;
		InstructionPointer m_return_ip;
		int m_arg_count = 0;
	};

	ExecutableModule m_module;
	ExecutableModule* m_current_module;
	std::array<Value, VALUE_STACK_MAX> m_value_stack;
	std::array<CallFrame, FRAME_STACK_MAX> m_call_stack;
	StackPointer<Value, VALUE_STACK_MAX> m_base_pointer = m_value_stack.begin();
	StackPointer<Value, VALUE_STACK_MAX> m_value_stack_pointer = m_value_stack.begin();
	StackPointer<CallFrame, FRAME_STACK_MAX> m_call_stack_pointer = m_call_stack.begin();
	InstructionPointer m_instruction_pointer;
	std::unordered_map<std::string, Value> m_global_vars;
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

		return m_module.GetConstant(index);
	}
	
	inline const std::string& ReadGlobalVariable()
	{
		int index = static_cast<int>(ReadByte());
		return m_module.GetGlobalVariable(index);
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

	inline static bool AreNumerical(const Value& left, const Value& right){return left.IsNumber() && right.IsNumber();}

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

	inline static bool AreSameType(const Value& left, const Value& right) { return Value::AreSameType(left, right); }

	void BinaryOperation(std::function<Value(const Value&, const Value&)>&& op, bool (*type_checker)(const Value&, const Value&));

	void InitializeNativeFunctions();
};
