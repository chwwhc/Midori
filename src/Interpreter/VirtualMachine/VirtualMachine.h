#pragma once

#include "Common/Value/Value.h"
#include "Common/ExecutableModule/ExecutableModule.h"

#include <array>
#include <functional>
#include <unordered_map>
#include <iostream>

#define STACK_MAX 512

class VirtualMachine
{

public:
	VirtualMachine(ExecutableModule&& module) : m_module(std::move(module)) {}

private:
	using StackPointer = std::array<Value, STACK_MAX>::iterator;

	ExecutableModule m_module;
	std::array<Value, STACK_MAX> m_value_stack;
	StackPointer m_stack_pointer = m_value_stack.begin();
	std::unordered_map<std::string, Value> m_global_vars;
	int m_instruction_pointer = 0;
	bool m_error = false;

public:
	void Execute();

private:
	inline OpCode ReadByte()
	{
		return static_cast<OpCode>(m_module.ReadByteCode(m_instruction_pointer++));
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
			if (m_stack_pointer == m_value_stack.end())
			{
				std::cerr << "Value stack overflow." << std::endl;
				m_error = true;
			}

			*m_stack_pointer++ = std::move(value);
		}
	}

	inline Value Pop()
	{
		return std::move(*(--m_stack_pointer));
	}

	inline const Value& Peek(int distance)
	{
		return *(m_stack_pointer - 1 - distance);
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
};
