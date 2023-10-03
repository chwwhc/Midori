#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"

#include <cmath>

void VirtualMachine::BinaryOperation(std::function<Value(const Value&, const Value&)>&& op, bool (*type_checker)(const Value&, const Value&))
{
	const Value& right_peek = Peek(0);
	const Value& left_peek = Peek(1);

	if (!type_checker(left_peek, right_peek))
	{
		std::cerr << "Operands must satisfy the type conditions." << std::endl;
		m_error = true;
		return;
	}

	Value right = Pop();
	Value left = Pop();

	Push(op(left, right));
}

void VirtualMachine::Execute()
{
	while (true)
	{
		std::cout << "          ";
		for (StackPointer it = m_value_stack.begin(); it < m_stack_pointer; ++it)
		{
			std::cout << "[ " << it->ToString() << " ]";
		}
		std::cout << std::endl;
		int dbg_instruction_pointer = m_instruction_pointer;
		Disassembler::DisassembleInstruction(m_module, dbg_instruction_pointer);

		OpCode instruction = ReadByte();

		switch (instruction)
		{
		case OpCode::CONSTANT:
		case OpCode::CONSTANT_LONG:
		case OpCode::CONSTANT_LONG_LONG:
		{
			Value constant = ReadConstant(instruction);
			Push(std::move(constant));
			break;
		}
		case OpCode::NIL:
		{
			Push(Value());
			break;
		}
		case OpCode::TRUE:
		{
			Push(Value(true));
			break;
		}
		case OpCode::FALSE:
		{
			Push(Value(false));
			break;
		}
		case OpCode::ARRAY_CREATE:
		{
			int count = ReadThreeBytes();
			std::vector<Value> arr;
			arr.reserve(count);
			for (int i = 0; i < count; i++)
			{
				arr.emplace_back(Pop());
			}
			std::reverse(arr.begin(), arr.end());
			Object* arr_object = new Object(std::move(arr));
			Push(arr_object);
			break;
		}
		case OpCode::ARRAY_GET:
		{
			if (!Peek(1).IsObjectPointer() || !Peek(1).GetObjectPointer()->IsArray())
			{
				std::cerr << "Operand must be an array." << std::endl;
				m_error = true;
				return;
			}

			Value index = Pop();
			Value arr = Pop();
			std::vector<Value>& arr_ref = arr.GetObjectPointer()->GetArray();

			if (!index.IsNumber())
			{
				std::cerr << "Index must be a number." << std::endl;
				m_error = true;
				return;
			}

			double index_as_double = index.GetNumber();
			if (index_as_double != static_cast<int>(index_as_double))
			{
				std::cerr << "Index must be an integer." << std::endl;
			}

			int i = static_cast<int>(index_as_double);
			if (i < 0 || i >= static_cast<int>(arr_ref.size()))
			{
				std::cerr << "Index out of bounds." << std::endl;
				m_error = true;
				return;
			}

			Push(Value(arr_ref[i]));
			break;
		}
		case OpCode::ARRAY_SET:
		{
			if (!Peek(2).IsObjectPointer() || !Peek(2).GetObjectPointer()->IsArray())
			{
				std::cerr << "Operand must be an array." << std::endl;
				m_error = true;
				return;
			}

			Value value = Pop();
			Value index = Pop();
			Value arr = Pop();
			std::vector<Value>& arr_ref = arr.GetObjectPointer()->GetArray();

			if (!index.IsNumber())
			{
				std::cerr << "Index must be a number." << std::endl;
				m_error = true;
				return;
			}

			double index_as_double = index.GetNumber();
			if (index_as_double != static_cast<int>(index_as_double))
			{
				std::cerr << "Index must be an integer." << std::endl;
			}

			int i = static_cast<int>(index_as_double);
			if (i < 0 || i >= static_cast<int>(arr_ref.size()))
			{
				std::cerr << "Index out of bounds." << std::endl;
				m_error = true;
				return;
			}

			arr_ref[i] = std::move(value);
			break;
		}
		case OpCode::ARRAY_RESERVE:
		{
			Value value = Pop();

			if (!value.IsNumber())
			{
				std::cerr << "Length must be a number." << std::endl;
				m_error = true;
				return;
			}

			double value_as_double = value.GetNumber();
			if (value_as_double < 0 || value_as_double > THREE_BYTE_MAX)
			{
				std::cerr << "Length must be between 0 and " << THREE_BYTE_MAX << "." << std::endl;
			}

			size_t size = static_cast<size_t>(value_as_double);
			Object* arr_object = new Object(std::vector<Value>(size));
			Push(arr_object);
			break;
		}
		case OpCode::LEFT_SHIFT:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) >> static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers);
			break;
		}
		case OpCode::RIGHT_SHIFT:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) >> static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers);
			break;
		}
		case OpCode::BITWISE_AND:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) & static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers);
			break;
		}
		case OpCode::BITWISE_OR:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) | static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers);
			break;
		}
		case OpCode::BITWISE_XOR:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) ^ static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers);
			break;
		}
		case OpCode::ADD:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() + right.GetNumber()); },
				AreNumerical);
			break;
		}
		case OpCode::SUBTRACT:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() - right.GetNumber()); },
				AreNumerical);
			break;
		}
		case OpCode::MULTIPLY:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() * right.GetNumber()); },
				AreNumerical);
			break;
		}
		case OpCode::DIVIDE:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() / right.GetNumber()); },
				AreNumerical);
			break;
		}
		case OpCode::MODULO:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(std::fmod(left.GetNumber(), right.GetNumber())); },
				AreNumerical);
			break;
		}
		case OpCode::EQUAL:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left == right); },
				AreSameType);
			break;
		}
		case OpCode::NOT_EQUAL:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left != right); },
				AreSameType);
			break;
		}
		case OpCode::GREATER:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() > right.GetNumber()); },
				AreNumerical);
			break;
		}
		case OpCode::GREATER_EQUAL:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() >= right.GetNumber()); },
				AreNumerical);
			break;
		}
		case OpCode::LESS:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() < right.GetNumber()); },
				AreNumerical);
			break;
		}
		case OpCode::LESS_EQUAL:
		{
			BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() <= right.GetNumber()); },
				AreNumerical);
			break;
		}
		case OpCode::NOT:
		{
			if (!Peek(0).IsBool())
			{
				std::cerr << "Operand must be a boolean.";
				m_error = true;
				return;
			}

			*(std::prev(m_stack_pointer)) = Value(!Peek(0).GetBool());
			break;
		}
		case OpCode::NEGATE:
		{
			if (!Peek(0).IsNumber())
			{
				std::cerr << "Operand must be a number.";
				m_error = true;
				return;
			}

			*(std::prev(m_stack_pointer)) = Value(-Peek(0).GetNumber());
			break;
		}
		case OpCode::JUMP_IF_FALSE:
		{
			int offset = ReadShort();
			if (!Peek(0).IsBool())
			{
				std::cerr << "Operand must be a boolean.";
				m_error = true;
				return;
			}
			if (!Peek(0).GetBool())
			{
				m_instruction_pointer += offset;
			}

			break;
		}
		case OpCode::JUMP_IF_TRUE:
		{
			int offset = ReadShort();
			if (!Peek(0).IsBool())
			{
				std::cerr << "Operand must be a boolean.";
				m_error = true;
				return;
			}
			if (Peek(0).GetBool())
			{
				m_instruction_pointer += offset;
			}

			break;
		}
		case OpCode::JUMP:
		{
			int offset = ReadShort();
			m_instruction_pointer += offset;
			break;
		}
		case OpCode::JUMP_BACK:
		{
			int offset = ReadShort();
			m_instruction_pointer -= offset;
			break;
		}
		case OpCode::CALL:
		{
			break;
		}
		case OpCode::DEFINE_GLOBAL:
		{
			const std::string& name = ReadGlobalVariable();
			m_global_vars[name] = Pop();
			break;
		}
		case OpCode::GET_GLOBAL:
		{
			const std::string& name = ReadGlobalVariable();
			std::unordered_map<std::string, Value>::iterator it = m_global_vars.find(name);
			if (it == m_global_vars.end())
			{
				std::cerr << "Undefined variable '" << name << "'.";
				m_error = true;
				return;
			}
			Push(Value(it->second));
			break;
		}
		case OpCode::SET_GLOBAL:
		{
			const std::string& name = ReadGlobalVariable();
			std::unordered_map<std::string, Value>::iterator it = m_global_vars.find(name);
			if (it == m_global_vars.end())
			{
				std::cerr << "Undefined variable '" << name << "'.";
				m_error = true;
				return;
			}
			it->second = Peek(0);
			break;
		}
		case OpCode::GET_LOCAL:
		{
			int offset = static_cast<int>(ReadByte());
			Value value_copy = m_value_stack[offset];
			Push(std::move(value_copy));
			break;
		}
		case OpCode::SET_LOCAL:
		{
			int offset = static_cast<int>(ReadByte());
			m_value_stack[offset] = Peek(0);
			break;
		}
		case OpCode::POP:
		{
			Pop();
			break;
		}
		case OpCode::POP_MULTIPLE:
		{
			int count = static_cast<int>(ReadByte());
			m_stack_pointer -= count;
			break;
		}
		case OpCode::PRINT:
		{
			Value value = Pop();
			std::cout << value.ToString() << std::endl;
			break;
		}
		case OpCode::RETURN:
		{
			return;
		}



		default:
		{
			std::cout << "Unknown instruction: " << static_cast<int>(instruction) << std::endl;
			return;
		}
		}
	}
}
