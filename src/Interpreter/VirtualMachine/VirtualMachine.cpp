#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"

#include <cmath>

void VirtualMachine::BinaryOperation(std::function<Value(const Value&, const Value&)>&& op, bool (*type_checker)(const Value&, const Value&))
{
	const Value& right_peek = Peek(0).m_value;
	const Value& left_peek = Peek(1).m_value;

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

Traceable::GarbageCollectionRoots VirtualMachine::GetValueStackGarbageCollectionRoots()
{
	Traceable::GarbageCollectionRoots roots;
	for (StackPointer<VirtualMachine::ValueSlot, VALUE_STACK_MAX> it = m_value_stack.begin(); it < m_value_stack_pointer; ++it)
	{
		if (it->m_value.IsObjectPointer())
		{
			roots.emplace(static_cast<Traceable*>(it->m_value.GetObjectPointer()));
		}
	}

	return roots;
}

Traceable::GarbageCollectionRoots VirtualMachine::GetGlobalGarbageCollectionRoots()
{
	Traceable::GarbageCollectionRoots roots;
	for (GlobalVariables::const_iterator it = m_global_vars.cbegin(); it != m_global_vars.cend(); ++it)
	{
		const Value& value = it->second;
		if (value.IsObjectPointer())
		{
			roots.emplace(static_cast<Traceable*>(value.GetObjectPointer()));
		}
	}

	return roots;
}

Traceable::GarbageCollectionRoots VirtualMachine::GetGarbageCollectionRoots()
{
	Traceable::GarbageCollectionRoots stack_value_roots = GetValueStackGarbageCollectionRoots();
	Traceable::GarbageCollectionRoots global_roots = GetGlobalGarbageCollectionRoots();
	
	stack_value_roots.insert(global_roots.begin(), global_roots.end());
	return stack_value_roots;
}

void VirtualMachine::CollectGarbage()
{
	if (Traceable::s_total_bytes_allocated - Traceable::s_static_bytes_allocated < GARBAGE_COLLECTION_THRESHOLD)
	{
		return;
	}

	Traceable::GarbageCollectionRoots roots = GetGarbageCollectionRoots();
	if (roots.empty())
	{
		return;
	}

#ifdef DEBUG
	std::cout << "\nBefore garbage collection:";
	Traceable::PrintMemoryTelemetry();
#endif
	m_garbage_collector.ReclaimMemory(std::move(roots));
#ifdef DEBUG
	std::cout << "\nAfter garbage collection:";
	Traceable::PrintMemoryTelemetry();
#endif
}

void VirtualMachine::Execute()
{
	NativeFunction::InitializeNativeFunctions(*this);

	while (m_instruction_pointer != m_current_bytecode->cend())
	{
#ifdef DEBUG
		std::cout << "          ";
		for (StackPointer<VirtualMachine::ValueSlot, VALUE_STACK_MAX> it = m_base_pointer; it < m_value_stack_pointer; ++it)
		{
			std::cout << "[ " << it->m_value.ToString() << " | " << (it->m_is_named ? "named" : "unnamed") << " ]";
		}
		std::cout << std::endl;
		int dbg_instruction_pointer = static_cast<int>(std::distance(m_current_bytecode->cbegin(), m_instruction_pointer));
		Disassembler::DisassembleInstruction(*m_current_bytecode, m_static_data, m_global_table, dbg_instruction_pointer);
#endif
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
		case OpCode::UNIT:
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
		case OpCode::CREATE_ARRAY:
		{
			int count = ReadThreeBytes();
			std::vector<Value> arr;
			arr.reserve(count);
			for (int i = 0; i < count; i++)
			{
				arr.emplace_back(Pop());
			}
			std::reverse(arr.begin(), arr.end());
			Object* arr_object = RuntimeAllocateObject(std::move(arr));
			Push(arr_object);
			break;
		}
		case OpCode::GET_ARRAY:
		{
			int num_indices = static_cast<int>(ReadByte());
			CheckIsArray(Peek(num_indices).m_value);
			if (m_error)
			{
				return;
			}

			std::vector<Value> indices;
			for (int i = 0; i < num_indices; i += 1)
			{
				indices.emplace_back(Pop());
			}

			std::reverse(indices.begin(), indices.end());

			Value arr = Pop();
			std::vector<Value>& arr_ref = arr.GetObjectPointer()->GetArray();

			for (Value& index : indices)
			{
				CheckIsNumber(index);
				if (m_error)
				{
					return;
				}

				double index_as_double = index.GetNumber();
				CheckIsInteger(index_as_double);
				if (m_error)
				{
					return;
				}

				int i = static_cast<int>(index_as_double);
				CheckIndexBounds(i, static_cast<int>(arr_ref.size()));
				if (m_error)
				{
					return;
				}

				Value next_val = arr_ref[i];

				if (&index != &indices.back())
				{
					CheckIsArray(next_val);
					if (m_error)
					{
						return;
					}

					arr_ref = next_val.GetObjectPointer()->GetArray();
				}
				else
				{
					Push(std::move(next_val));
				}
			}
			break;
		}
		case OpCode::SET_ARRAY:
		{
			int num_indices = static_cast<int>(ReadByte());
			CheckIsArray(Peek(num_indices + 1).m_value);
			if (m_error)
			{
				return;
			}

			Value value_to_set = Pop();
			std::vector<Value> indices;

			for (int i = 0; i < num_indices; ++i)
			{
				indices.emplace_back(Pop());
			}

			std::reverse(indices.begin(), indices.end());

			Value arr = Pop();
			std::vector<Value>& arr_ref = arr.GetObjectPointer()->GetArray();

			for (Value& index : indices)
			{
				CheckIsNumber(index);
				if (m_error)
				{
					return;
				}

				double index_as_double = index.GetNumber();
				CheckIsInteger(index_as_double);
				if (m_error)
				{
					return;
				}

				int i = static_cast<int>(index_as_double);
				CheckIndexBounds(i, static_cast<int>(arr_ref.size()));
				if (m_error)
				{
					return;
				}

				if (&index != &indices.back())
				{
					CheckIsArray(arr_ref[i]);
					if (m_error)
					{
						return;
					}

					arr_ref = arr_ref[i].GetObjectPointer()->GetArray();
				}
				else
				{
					arr_ref[i] = value_to_set;
				}
			}

			Push(std::move(value_to_set));
			break;
		}
		case OpCode::RESERVE_ARRAY:
		{
			Value value = Pop();

			CheckIsNumber(value);
			if (m_error)
			{
				return;
			}

			double value_as_double = value.GetNumber();
			CheckArrayMaxLength(value_as_double);
			if (m_error)
			{
				return;
			}

			size_t size = static_cast<size_t>(value_as_double);
			Object* arr_object = RuntimeAllocateObject(std::vector<Value>(size));
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
		case OpCode::CONCAT:
		{
			BinaryOperation([this](const Value& left, const Value& right)
				{
					Object* left_value = left.GetObjectPointer();
					Object* right_value = right.GetObjectPointer();

					if (left_value->IsArray()) {
						const std::vector<Value>& left_value_vector_ref = left_value->GetArray();
						const std::vector<Value>& right_value_vector_ref = right_value->GetArray();

						std::vector<Value> new_value_vector(left_value_vector_ref);
						new_value_vector.insert(new_value_vector.end(),
							right_value_vector_ref.begin(),
							right_value_vector_ref.end());

						return RuntimeAllocateObject(std::move(new_value_vector));
					}
					else {
						const std::string& left_value_string_ref = left_value->GetString();
						const std::string& right_value_string_ref = right_value->GetString();

						std::string new_value_string = left_value_string_ref + right_value_string_ref;

						return RuntimeAllocateObject(std::move(new_value_string));
					}
				},
				AreConcatenatable);
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
			if (!Peek(0).m_value.IsBool())
			{
				std::cerr << "Operand must be a boolean.";
				m_error = true;
				return;
			}

			(std::prev(m_value_stack_pointer))->m_value = Value(!Peek(0).m_value.GetBool());
			break;
		}
		case OpCode::NEGATE:
		{
			if (!Peek(0).m_value.IsNumber())
			{
				std::cerr << "Operand must be a number.";
				m_error = true;
				return;
			}

			(std::prev(m_value_stack_pointer))->m_value = Value(-Peek(0).m_value.GetNumber());
			break;
		}
		case OpCode::JUMP_IF_FALSE:
		{
			int offset = ReadShort();
			if (!Peek(0).m_value.IsBool())
			{
				std::cerr << "Operand must be a boolean.";
				m_error = true;
				return;
			}
			if (!Peek(0).m_value.GetBool())
			{
				m_instruction_pointer += offset;
			}

			break;
		}
		case OpCode::JUMP_IF_TRUE:
		{
			int offset = ReadShort();
			if (!Peek(0).m_value.IsBool())
			{
				std::cerr << "Operand must be a boolean.";
				m_error = true;
				return;
			}
			if (Peek(0).m_value.GetBool())
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
			int arity = static_cast<int>(ReadByte());
			const Value& function = Peek(arity).m_value;

			if (!function.IsObjectPointer())
			{
				std::cerr << "Not a callable.\n";
				m_error = true;
				return;
			}

			Object* function_ptr = function.GetObjectPointer();

			// Native Function
			if (function_ptr->IsNativeFunction())
			{
				Object::NativeFunction& native_function = function.GetObjectPointer()->GetNativeFunction();

				if (arity != native_function.m_arity)
				{
					std::cerr << "Incorrect arity";
					m_error = true;
					return;
				}

				native_function.m_cpp_function();
				m_value_stack_pointer -= arity;
			}
			else if (function_ptr->IsClosure())
			{
				Object::Closure& closure = function.GetObjectPointer()->GetClosure();

				if (arity != closure.m_function.m_arity)
				{
					std::cerr << "Incorrect arity";
					m_error = true;
					return;
				}

				m_closures.emplace_back(&closure);

				// Return address := pop all the arguments and the callee
				*m_call_stack_pointer++ = { m_current_bytecode , m_base_pointer, m_value_stack_pointer - arity - 1, m_instruction_pointer };

				m_current_bytecode = &closure.m_function.m_bytecode;
				m_instruction_pointer = m_current_bytecode->cbegin();
				m_base_pointer = m_value_stack_pointer - arity;
			}

			break;
		}
		case OpCode::CREATE_CLOSURE:
		{
			Object::Closure& closure = Peek(0).m_value.GetObjectPointer()->GetClosure();
			closure.m_cell_values.clear();
			std::vector<Object*>& captured_variables = closure.m_cell_values;

			if (!m_closures.empty())
			{
				for (Object* enclosed_cell_value : m_closures.back()->m_cell_values)
				{
					captured_variables.emplace_back(RuntimeAllocateObject(Object::CellValue(enclosed_cell_value->GetCellValue())));
				}
			}

			for (StackPointer<VirtualMachine::ValueSlot, VALUE_STACK_MAX> it = m_base_pointer; it < m_value_stack_pointer; ++it)
			{
				if (it->m_is_named)
				{
					captured_variables.emplace_back(RuntimeAllocateObject(Object::CellValue(it->m_value)));
				}
			}

			break;
		}
		case OpCode::GET_NATIVE:
		{
			const std::string& name = ReadGlobalVariable();
			GlobalVariables::iterator it = m_global_vars.find(name);
			if (it == m_global_vars.end())
			{
				std::cerr << "Undefined variable '" << name << "'.";
				m_error = true;
				return;
			}
			Push(Value(it->second));
			break;
		}
		case OpCode::GET_LOCAL:
		{
			int offset = static_cast<int>(ReadByte());
			Value value_copy = (m_base_pointer + offset)->m_value;
			Push(std::move(value_copy));
			break;
		}
		case OpCode::SET_LOCAL:
		{
			int offset = static_cast<int>(ReadByte());
			(m_base_pointer + offset)->m_value = Peek(0).m_value;
			break;
		}
		case OpCode::GET_CELL:
		{
			int offset = static_cast<int>(ReadByte());
			Value value_copy = m_closures.back()->m_cell_values[offset]->GetCellValue().m_value;
			Push(std::move(value_copy));
			break;
		}
		case OpCode::SET_CELL:
		{
			int offset = static_cast<int>(ReadByte());
			m_closures.back()->m_cell_values[offset]->GetCellValue().m_value = Peek(0).m_value;
			break;
		}
		case OpCode::DEFINE_NAME:
		{
			m_value_stack_pointer->m_is_named = true;
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
			m_value_stack_pointer -= count;
			break;
		}
		case OpCode::RETURN:
		{
			CallFrame& top_frame = *(--m_call_stack_pointer);
			m_closures.pop_back();

			Value return_val = Pop();

			m_current_bytecode = top_frame.m_return_module;
			m_base_pointer = top_frame.m_return_bp;
			m_instruction_pointer = top_frame.m_return_ip;
			m_value_stack_pointer = top_frame.m_return_sp;

			Push(std::move(return_val));
			break;
		}
		case OpCode::HALT:
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
