#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"

#include <cmath>
#include <algorithm>

#define EXECUTE_OR_ABORT(operation)  \
    m_last_result = operation;       \
    if (!m_last_result.has_value())	 \
	{								 \
        return m_last_result;        \
    }

MidoriResult::InterpreterResult VirtualMachine::ProcessIndicesAndPerformChecks(std::vector<Value>& indices, Value& arr)
{
	std::vector<Value>& arr_ref = arr.GetObjectPointer()->GetArray();
	std::vector<std::function<MidoriResult::InterpreterResult(Value&)>> checks =
	{
		[this](Value& val) { return CheckIsNumber(&val); },
		[this](Value& val) { return CheckIsInteger(&val); },
		[&arr_ref, this](Value& val) { return CheckIndexBounds(&val, static_cast<int>(arr_ref.size())); }
	};

	std::for_each(indices.begin(), indices.end(), [&checks, &arr_ref, &indices, this](Value& index)
		{
			std::for_each(checks.begin(), checks.end(), [&index, &arr_ref, &indices, this](std::function<MidoriResult::InterpreterResult(Value&)>& check)
				{
					EXECUTE_OR_ABORT(check(index))
				});

			Value& next_val = arr_ref[static_cast<int>(index.GetNumber())];

			if (&index != &indices.back())
			{
				EXECUTE_OR_ABORT(CheckIsArray(&next_val))
					arr_ref = next_val.GetObjectPointer()->GetArray();
			}
			else
			{
				Push(std::move(next_val));
			}
		});

	return &arr;
}

MidoriResult::InterpreterResult VirtualMachine::BinaryOperation(std::function<Value(const Value&, const Value&)>&& op, bool (*type_checker)(const Value&, const Value&))
{
	return Bind(Peek(0), [&type_checker, &op, this](Value* right_peek) -> MidoriResult::InterpreterResult
		{
			return Bind(Peek(1), [&right_peek, &type_checker, &op, this](Value* left_peek) -> MidoriResult::InterpreterResult
				{
					if (!type_checker(*left_peek, *right_peek))
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operands must satisfy the type conditions", GetLine()));
					}
					else
					{
						return Bind(Pop(), [&op, this](Value* right)
							{
								return Bind(Pop(), [&op, &right, this](Value* left) -> MidoriResult::InterpreterResult
									{
										return Push(op(*left, *right));
									});
							});
					}
				});
		});
}

Traceable::GarbageCollectionRoots VirtualMachine::GetValueStackGarbageCollectionRoots()
{
	Traceable::GarbageCollectionRoots roots;

	std::for_each_n(m_value_stack.begin(), std::distance(m_value_stack.begin(), m_value_stack_pointer), [&roots](VirtualMachine::ValueSlot& valueSlot) 
		{
			if (valueSlot.m_is_named && valueSlot.m_value.IsObjectPointer()) 
			{
				roots.emplace(static_cast<Traceable*>(valueSlot.m_value.GetObjectPointer()));
			}
		});

	return roots;
}

Traceable::GarbageCollectionRoots VirtualMachine::GetGarbageCollectionRoots()
{
	return GetValueStackGarbageCollectionRoots();
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

MidoriResult::InterpreterResult VirtualMachine::Execute()
{
	NativeFunction::InitializeNativeFunctions(*this);

	while (m_instruction_pointer != m_current_bytecode->cend())
	{
#ifdef DEBUG
		std::cout << "          ";
		for (StackPointer<VirtualMachine::ValueSlot, VALUE_STACK_MAX> it = m_base_pointer; it < m_value_stack_pointer; ++it)
		{
			std::cout << "[ " << it->m_value.ToString() << " | " << (it->m_is_named ? 'L' : 'R') << " ]";
		}
		std::cout << std::endl;
		int dbg_instruction_pointer = static_cast<int>(std::distance(m_current_bytecode->cbegin(), m_instruction_pointer));
		Disassembler::DisassembleInstruction(*m_current_bytecode, m_executable_module.m_static_data, m_executable_module.m_global_table, dbg_instruction_pointer);
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
			std::vector<Value> arr(count);
			for (int i = count - 1; i >= 0; i -= 1)
			{
				EXECUTE_OR_ABORT(Pop())
				arr[i] = std::move(*m_last_result.value());
			}
			Traceable* arr_object = RuntimeAllocateObject(std::move(arr));
			EXECUTE_OR_ABORT(Push(arr_object))
			break;
		}
		case OpCode::GET_ARRAY:
		{
			int num_indices = static_cast<int>(ReadByte());
			EXECUTE_OR_ABORT(Bind(Peek(num_indices), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					return CheckIsArray(value);
				}))

				std::vector<Value> indices(num_indices);
				for (int i = num_indices - 1; i >= 0; i -= 1)
				{
					EXECUTE_OR_ABORT(Bind(Pop(), [&indices, i](Value* value) -> MidoriResult::InterpreterResult
						{
							indices[i] = *value;
							return value;
						}))
				}

				EXECUTE_OR_ABORT(Bind(Pop(), [&indices, this](Value* arr) -> MidoriResult::InterpreterResult
					{
						return ProcessIndicesAndPerformChecks(indices, *arr);
					}))
					break;
		}
		case OpCode::SET_ARRAY:
		{
			int num_indices = static_cast<int>(ReadByte());
			EXECUTE_OR_ABORT(Bind(Peek(num_indices + 1), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					return CheckIsArray(value);
				}))

				EXECUTE_OR_ABORT(Pop())
					Value& value_to_set = *m_last_result.value();

				std::vector<Value> indices(num_indices);
				for (int i = num_indices - 1; i >= 0; i -= 1)
				{
					EXECUTE_OR_ABORT(Bind(Pop(), [&indices, i](Value* value)
						{
							indices[i] = *value;
							return value;
						}))
				}

				EXECUTE_OR_ABORT(Pop())
					Value& arr = *m_last_result.value();

				EXECUTE_OR_ABORT(ProcessIndicesAndPerformChecks(indices, arr))

					int final_index = static_cast<int>(indices.back().GetNumber());
				arr.GetObjectPointer()->GetArray()[final_index] = value_to_set;

				Push(std::move(value_to_set));

				break;
		}
		case OpCode::RESERVE_ARRAY:
		{
			EXECUTE_OR_ABORT(Bind(Pop(), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					return Bind(CheckIsNumber(value), [this](Value* value) -> MidoriResult::InterpreterResult
						{
							return CheckArrayMaxLength(value);
						});
				}))

			size_t size = static_cast<size_t>(m_last_result.value()->GetNumber());
			Traceable* arr_object = RuntimeAllocateObject(std::vector<Value>(size));
			EXECUTE_OR_ABORT(Push(arr_object))
			break;
		}
		case OpCode::LEFT_SHIFT:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) << static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers))
				break;
		}
		case OpCode::RIGHT_SHIFT:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) >> static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers))
				break;
		}
		case OpCode::BITWISE_AND:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) & static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers))
				break;
		}
		case OpCode::BITWISE_OR:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) | static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers))
				break;
		}
		case OpCode::BITWISE_XOR:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(static_cast<double>(static_cast<int>(left.GetNumber()) ^ static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers))
				break;
		}
		case OpCode::ADD:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() + right.GetNumber()); },
				AreNumerical))
				break;
		}
		case OpCode::SUBTRACT:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() - right.GetNumber()); },
				AreNumerical))
				break;
		}
		case OpCode::MULTIPLY:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() * right.GetNumber()); },
				AreNumerical))
				break;
		}
		case OpCode::DIVIDE:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() / right.GetNumber()); },
				AreNumerical))
				break;
		}
		case OpCode::MODULO:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(std::fmod(left.GetNumber(), right.GetNumber())); },
				AreNumerical))
				break;
		}
		case OpCode::CONCAT:
		{
			EXECUTE_OR_ABORT(BinaryOperation([this](const Value& left, const Value& right)
				{
					Traceable* left_value = left.GetObjectPointer();
					Traceable* right_value = right.GetObjectPointer();

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
				AreConcatenatable))
				break;
		}
		case OpCode::EQUAL:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left == right); },
				AreSameType))
				break;
		}
		case OpCode::NOT_EQUAL:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left != right); },
				AreSameType))
				break;
		}
		case OpCode::GREATER:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() > right.GetNumber()); },
				AreNumerical))
				break;
		}
		case OpCode::GREATER_EQUAL:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() >= right.GetNumber()); },
				AreNumerical))
				break;
		}
		case OpCode::LESS:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() < right.GetNumber()); },
				AreNumerical))
				break;
		}
		case OpCode::LESS_EQUAL:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const Value& left, const Value& right)
				{ return Value(left.GetNumber() <= right.GetNumber()); },
				AreNumerical))
				break;
		}
		case OpCode::NOT:
		{
			EXECUTE_OR_ABORT(Bind(Peek(0), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsBool())
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a boolean.", GetLine()));
					}
				}))

				(std::prev(m_value_stack_pointer))->m_value = Value(!m_last_result.value()->GetBool());
				break;
		}
		case OpCode::NEGATE:
		{
			EXECUTE_OR_ABORT(Bind(Peek(0), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsNumber())
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a number.", GetLine()));
					}
				}))

				(std::prev(m_value_stack_pointer))->m_value = Value(-m_last_result.value()->GetNumber());
				break;
		}
		case OpCode::JUMP_IF_FALSE:
		{
			int offset = ReadShort();
			EXECUTE_OR_ABORT(Bind(Peek(0), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsBool())
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a boolean.", GetLine()));
					}
				}))

				if (!m_last_result.value()->GetBool())
				{
					m_instruction_pointer += offset;
				}

				break;
		}
		case OpCode::JUMP_IF_TRUE:
		{
			int offset = ReadShort();
			EXECUTE_OR_ABORT(Bind(Peek(0), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsBool())
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a boolean.", GetLine()));
					}
				}))

				if (m_last_result.value()->GetBool())
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
			EXECUTE_OR_ABORT(Bind(Peek(arity), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsNativeFunction() || (value->IsObjectPointer() && value->GetObjectPointer()->IsClosure()))
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a callable.", GetLine()));
					}
				}))
				const Value& callable = *m_last_result.value();

				// Native function
				if (callable.IsNativeFunction())
				{
					Value::NativeFunction& native_function = callable.GetNativeFunction();

					if (arity != native_function.m_arity)
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Incorrect arity", GetLine()));
					}

					native_function.m_cpp_function();
					m_value_stack_pointer -= arity;
				}
				else if (callable.IsObjectPointer() && callable.GetObjectPointer()->IsClosure())
				{
					Traceable::Closure& closure = callable.GetObjectPointer()->GetClosure();

					if (arity != closure.m_arity)
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Incorrect arity", GetLine()));
					}

					m_closures.emplace_back(&closure);

					// Return address := pop all the arguments and the callee
					*m_call_stack_pointer++ = { m_current_bytecode , m_base_pointer, m_value_stack_pointer - arity - 1, m_instruction_pointer };

					m_current_bytecode = &m_executable_module.m_modules[closure.m_bytecode_index];
					m_instruction_pointer = m_current_bytecode->cbegin();
					m_base_pointer = m_value_stack_pointer - arity;
					std::for_each_n(m_base_pointer, std::distance(m_base_pointer, m_value_stack_pointer), [](VirtualMachine::ValueSlot& valueSlot) 
						{
							if (valueSlot.m_value.IsObjectPointer() && valueSlot.m_value.GetObjectPointer()->IsCellValue()) 
							{
								valueSlot.m_value = valueSlot.m_value.GetObjectPointer()->GetCellValue().m_value;
							}
						});
				}

				break;
		}
		case OpCode::CREATE_CLOSURE:
		{
			EXECUTE_OR_ABORT(Bind(Peek(0), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsObjectPointer() && value->GetObjectPointer()->IsClosure())
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a closure.", GetLine()));
					}
				}))

				Value& closure_value = *m_last_result.value();
				Traceable* new_closure = RuntimeAllocateObject(Traceable::Closure(closure_value.GetObjectPointer()->GetClosure()));
				Traceable* old_closure = closure_value.GetObjectPointer();
				Traceable::Closure& new_closure_ref = new_closure->GetClosure();
				Traceable::Closure& old_closure_ref = old_closure->GetClosure();
				new_closure_ref.m_cell_values.clear();
				old_closure_ref.m_cell_values.clear();
				std::vector<Traceable*>& new_captured_variables = new_closure_ref.m_cell_values;
				std::vector<Traceable*>& old_captured_variables = old_closure_ref.m_cell_values;

				if (!m_closures.empty())
				{
					std::for_each(m_closures.back()->m_cell_values.begin(), m_closures.back()->m_cell_values.end(), [&new_captured_variables, &old_captured_variables](Traceable* enclosed_cell_value) 
						{
							new_captured_variables.emplace_back(enclosed_cell_value);
							old_captured_variables.emplace_back(enclosed_cell_value);
						});
				}

				std::for_each_n(m_base_pointer, std::distance(m_base_pointer, m_value_stack_pointer), [&new_captured_variables, &old_captured_variables, this](VirtualMachine::ValueSlot& valueSlot) 
					{
						if (valueSlot.m_is_named)
						{
							if (valueSlot.m_value.IsObjectPointer() && valueSlot.m_value.GetObjectPointer()->IsCellValue())
							{
								new_captured_variables.emplace_back(valueSlot.m_value.GetObjectPointer());
								old_captured_variables.emplace_back(valueSlot.m_value.GetObjectPointer());
							}
							else
							{
								Traceable* cell_value = RuntimeAllocateObject(Traceable::CellValue(std::move(valueSlot.m_value)));
								valueSlot.m_value = Value(cell_value);
								new_captured_variables.emplace_back(cell_value);
								old_captured_variables.emplace_back(cell_value);
							}
						}
					});

				closure_value = Value(new_closure);
				break;
		}
		case OpCode::GET_NATIVE:
		{
			const std::string& name = ReadGlobalVariable();
			GlobalVariables::iterator it = m_global_vars.find(name);
			if (it == m_global_vars.end())
			{
				return std::unexpected<std::string>(GenerateRuntimeError("Undefined variable '" + name + "'.", GetLine()));
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
			Value& value = (m_base_pointer + offset)->m_value;
			EXECUTE_OR_ABORT(Bind(Pop(), [&value](Value* value_to_set)
				{
					if (value.IsObjectPointer() && value.GetObjectPointer()->IsCellValue())
					{
						value.GetObjectPointer()->GetCellValue().m_value = std::move(*value_to_set);
					}
					else
					{
						value = std::move(*value_to_set);
					}

					return &value;
				}))
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
			EXECUTE_OR_ABORT(Bind(Peek(0), [this](Value* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsObjectPointer() && value->GetObjectPointer()->IsCellValue())
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a cell value.", GetLine()));
					}
				}))

				m_closures.back()->m_cell_values[offset]->GetCellValue().m_value = m_last_result.value();
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

			EXECUTE_OR_ABORT(Pop())
			Value& return_val = *m_last_result.value();

			m_current_bytecode = top_frame.m_return_module;
			m_base_pointer = top_frame.m_return_bp;
			m_instruction_pointer = top_frame.m_return_ip;
			m_value_stack_pointer = top_frame.m_return_sp;

			Push(std::move(return_val));
			break;
		}
		case OpCode::HALT:
		{
			return nullptr;
		}
		default:
		{
			return std::unexpected<std::string>(GenerateRuntimeError("Unknown instruction: " + std::to_string(static_cast<int>(instruction)), GetLine()));
		}
		}
	}

	return m_last_result;
}
