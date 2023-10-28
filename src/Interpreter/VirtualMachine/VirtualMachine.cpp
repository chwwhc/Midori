#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"

#include <cmath>
#include <algorithm>

#ifdef DEBUG
#include <iostream>
#endif

#define EXECUTE_OR_ABORT(operation)  \
    m_last_result = operation;       \
    if (!m_last_result.has_value())	 \
        return m_last_result

MidoriResult::InterpreterResult VirtualMachine::BinaryOperation(std::function<MidoriValue(const MidoriValue&, const MidoriValue&)>&& op, bool (*type_checker)(const MidoriValue&, const MidoriValue&))
{
	return Bind(Pop(), [&op, type_checker, this](MidoriValue* right) -> MidoriResult::InterpreterResult
		{
			return Bind(Pop(), [&op, type_checker, this, &right](MidoriValue* left) -> MidoriResult::InterpreterResult
				{
					if (!type_checker(*left, *right))
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operands must satisfy the type conditions", GetLine()));
					}
					else
					{
						return Bind(Push(std::move(*left)), [&op, &right, &left, this](MidoriValue*) -> MidoriResult::InterpreterResult
							{
								return Bind(Push(std::move(*right)), [&op, &right, this](MidoriValue*) -> MidoriResult::InterpreterResult
									{
										return Bind(Pop(), [&op, &right, this](MidoriValue* right) -> MidoriResult::InterpreterResult
											{
												return Bind(Pop(), [&op, &right, this](MidoriValue* left) -> MidoriResult::InterpreterResult
													{
														return Push(op(*left, *right));
													});
											});
									});
							});
					}
				});
		});
}

Traceable::GarbageCollectionRoots VirtualMachine::GetValueStackGarbageCollectionRoots()
{
	Traceable::GarbageCollectionRoots roots;

	std::for_each_n(m_value_stack.begin(), std::distance(m_value_stack.begin(), m_value_stack_pointer), [&roots](MidoriValue& value) -> void
		{
			if (value.IsObjectPointer())
			{
				roots.emplace(static_cast<Traceable*>(value.GetObjectPointer()));
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
		for (StackPointer<MidoriValue, VALUE_STACK_MAX> it = m_base_pointer; it < m_value_stack_pointer; ++it)
		{
			std::cout << "[ " << it->ToString() << " ]";
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
			MidoriValue constant = ReadConstant(instruction);
			Push(std::move(constant));
			break;
		}
		case OpCode::UNIT:
		{
			Push(MidoriValue());
			break;
		}
		case OpCode::TRUE:
		{
			Push(MidoriValue(true));
			break;
		}
		case OpCode::FALSE:
		{
			Push(MidoriValue(false));
			break;
		}
		case OpCode::CREATE_ARRAY:
		{
			int count = ReadThreeBytes();
			std::vector<MidoriValue> arr(count);
			for (int i = count - 1; i >= 0; i -= 1)
			{
				EXECUTE_OR_ABORT(Pop());
				arr[i] = std::move(*m_last_result.value());
			}
			Traceable* arr_object = RuntimeAllocateObject(std::move(arr));
			EXECUTE_OR_ABORT(Push(arr_object));
			break;
		}
		case OpCode::GET_ARRAY:
		{
			int num_indices = static_cast<int>(ReadByte());

			std::vector<MidoriValue> indices(num_indices);
			for (int i = num_indices - 1; i >= 0; i -= 1)
			{
				EXECUTE_OR_ABORT(Pop());
				indices[i] = std::move(*m_last_result.value());
			}

			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					return CheckIsArray(value);
				}));
			MidoriValue& arr = *m_last_result.value();
			std::vector<MidoriValue>& arr_ref = arr.GetObjectPointer()->GetArray();
			int arr_size = static_cast<int>(arr_ref.size());

			for (MidoriValue& index : indices)
			{
				EXECUTE_OR_ABORT(Bind(CheckIsNumber(&index), [arr_size, this](MidoriValue* value) -> MidoriResult::InterpreterResult
					{
						return Bind(CheckIsInteger(value), [arr_size, this](MidoriValue* value) -> MidoriResult::InterpreterResult
							{
								return CheckIndexBounds(value, arr_size);
							});
					}));

				MidoriValue& next_val = arr_ref[static_cast<size_t>(index.GetNumber())];

				if (&index != &indices.back())
				{
					EXECUTE_OR_ABORT(CheckIsArray(&next_val));
					arr_ref = next_val.GetObjectPointer()->GetArray();
				}
				else
				{
					EXECUTE_OR_ABORT(Push(std::move(next_val)));
				}
			}
			break;
		}
		case OpCode::SET_ARRAY:
		{
			int num_indices = static_cast<int>(ReadByte());

			EXECUTE_OR_ABORT(Pop());
			MidoriValue& value_to_set = *m_last_result.value();

			std::vector<MidoriValue> indices(num_indices);
			for (int i = num_indices - 1; i >= 0; i -= 1)
			{
				EXECUTE_OR_ABORT(Pop());
				indices[i] = std::move(*m_last_result.value());
			}

			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					return CheckIsArray(value);
				}));
			MidoriValue& arr = *m_last_result.value();
			std::vector<MidoriValue>& arr_ref = arr.GetObjectPointer()->GetArray();
			int arr_size = static_cast<int>(arr_ref.size());

			for (MidoriValue& index : indices)
			{
				EXECUTE_OR_ABORT(Bind(CheckIsNumber(&index), [arr_size, this](MidoriValue* value) -> MidoriResult::InterpreterResult
					{
						return Bind(CheckIsInteger(value), [arr_size, this](MidoriValue* value) -> MidoriResult::InterpreterResult
							{
								return CheckIndexBounds(value, arr_size);
							});
					}));
				size_t i = static_cast<size_t>(index.GetNumber());

				if (&index != &indices.back())
				{
					EXECUTE_OR_ABORT(CheckIsArray(&arr_ref[i]));
					arr_ref = arr_ref[i].GetObjectPointer()->GetArray();
				}
				else
				{
					arr_ref[i] = value_to_set;
				}
			}

			EXECUTE_OR_ABORT(Push(std::move(value_to_set)));
			break;
		}
		case OpCode::RESERVE_ARRAY:
		{
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					return Bind(CheckIsNumber(value), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
						{
							return CheckArrayMaxLength(value);
						});
				}));

			size_t size = static_cast<size_t>(m_last_result.value()->GetNumber());
			Traceable* arr_object = RuntimeAllocateObject(std::vector<MidoriValue>(size));
			EXECUTE_OR_ABORT(Push(arr_object));
			break;
		}
		case OpCode::LEFT_SHIFT:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(static_cast<double>(static_cast<int>(left.GetNumber()) << static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers));
			break;
		}
		case OpCode::RIGHT_SHIFT:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(static_cast<double>(static_cast<int>(left.GetNumber()) >> static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers));
			break;
		}
		case OpCode::BITWISE_AND:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(static_cast<double>(static_cast<int>(left.GetNumber()) & static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers));
			break;
		}
		case OpCode::BITWISE_OR:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(static_cast<double>(static_cast<int>(left.GetNumber()) | static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers));
			break;
		}
		case OpCode::BITWISE_XOR:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(static_cast<double>(static_cast<int>(left.GetNumber()) ^ static_cast<int>(right.GetNumber()))); },
				Are32BitIntegers));
			break;
		}
		case OpCode::ADD:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left.GetNumber() + right.GetNumber()); },
				AreNumerical));
			break;
		}
		case OpCode::SUBTRACT:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left.GetNumber() - right.GetNumber()); },
				AreNumerical));
			break;
		}
		case OpCode::MULTIPLY:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left.GetNumber() * right.GetNumber()); },
				AreNumerical));
			break;
		}
		case OpCode::DIVIDE:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left.GetNumber() / right.GetNumber()); },
				AreNumerical));
			break;
		}
		case OpCode::MODULO:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(std::fmod(left.GetNumber(), right.GetNumber())); },
				AreNumerical));
			break;
		}
		case OpCode::CONCAT:
		{
			EXECUTE_OR_ABORT(BinaryOperation([this](const MidoriValue& left, const MidoriValue& right)
				{
					Traceable* left_value = left.GetObjectPointer();
					Traceable* right_value = right.GetObjectPointer();

					if (left_value->IsArray()) {
						const std::vector<MidoriValue>& left_value_vector_ref = left_value->GetArray();
						const std::vector<MidoriValue>& right_value_vector_ref = right_value->GetArray();

						std::vector<MidoriValue> new_value_vector(left_value_vector_ref);
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
				AreConcatenatable));
			break;
		}
		case OpCode::EQUAL:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left == right); },
				AreSameType));
			break;
		}
		case OpCode::NOT_EQUAL:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left != right); },
				AreSameType));
			break;
		}
		case OpCode::GREATER:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left.GetNumber() > right.GetNumber()); },
				AreNumerical));
			break;
		}
		case OpCode::GREATER_EQUAL:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left.GetNumber() >= right.GetNumber()); },
				AreNumerical));
			break;
		}
		case OpCode::LESS:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left.GetNumber() < right.GetNumber()); },
				AreNumerical));
			break;
		}
		case OpCode::LESS_EQUAL:
		{
			EXECUTE_OR_ABORT(BinaryOperation([](const MidoriValue& left, const MidoriValue& right)
				{ return MidoriValue(left.GetNumber() <= right.GetNumber()); },
				AreNumerical));
			break;
		}
		case OpCode::NOT:
		{
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsBool())
					{
						return Push(MidoriValue(!value->GetBool()));
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a boolean.", GetLine()));
					}
				}));
			break;
		}
		case OpCode::NEGATE:
		{
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsNumber())
					{
						return Push(MidoriValue(-value->GetNumber()));
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a number.", GetLine()));
					}
				}));
			break;
		}
		case OpCode::JUMP_IF_FALSE:
		{
			int offset = ReadShort();
			EXECUTE_OR_ABORT(Bind(Pop(), [offset, this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsBool())
					{
						if (!value->GetBool())
						{
							m_instruction_pointer += offset;
						}
						return Push(std::move(*value));
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a boolean.", GetLine()));
					}
				}));
			break;
		}
		case OpCode::JUMP_IF_TRUE:
		{
			int offset = ReadShort();
			EXECUTE_OR_ABORT(Bind(Pop(), [offset, this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsBool())
					{
						if (value->GetBool())
						{
							m_instruction_pointer += offset;
						}
						return Push(std::move(*value));
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a boolean.", GetLine()));
					}
				}));
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
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsNativeFunction() || (value->IsObjectPointer() && value->GetObjectPointer()->IsClosure()))
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a callable.", GetLine()));
					}
				}));
			const MidoriValue& callable = *m_last_result.value();

			int arity = static_cast<int>(ReadByte());

			// Native function
			if (callable.IsNativeFunction())
			{
				MidoriValue::NativeFunction& native_function = callable.GetNativeFunction();

				if (arity != native_function.m_arity)
				{
					return std::unexpected<std::string>(GenerateRuntimeError("Incorrect arity", GetLine()));
				}

				native_function.m_cpp_function();
			}
			else if (callable.IsObjectPointer() && callable.GetObjectPointer()->IsClosure())
			{
				Traceable::Closure& closure = callable.GetObjectPointer()->GetClosure();

				if (arity != closure.m_arity)
				{
					return std::unexpected<std::string>(GenerateRuntimeError("Incorrect arity", GetLine()));
				}

				m_closure_stack.emplace(&closure);

				if (std::next(m_call_stack_pointer) == m_call_stack.end())
				{
					return std::unexpected<std::string>(GenerateRuntimeError("Call stack overflow.", GetLine()));
				}

				// Return address := pop all the arguments and the callee
				*m_call_stack_pointer++ = { m_current_bytecode , m_base_pointer, m_value_stack_pointer - arity, m_instruction_pointer };
				m_current_bytecode = &m_executable_module.m_modules[closure.m_bytecode_index];
				m_instruction_pointer = m_current_bytecode->cbegin();
				m_base_pointer = m_value_stack_pointer - arity;


				std::for_each_n(m_base_pointer, std::distance(m_base_pointer, m_value_stack_pointer), [](MidoriValue& value) -> void
					{
						if (value.IsObjectPointer() && value.GetObjectPointer()->IsCellValue())
						{
							value = value.GetObjectPointer()->GetCellValue().m_value;
						}
					});
			}

			break;
		}
		case OpCode::CREATE_CLOSURE:
		{
			int count = static_cast<int>(ReadByte());
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					if (value->IsObjectPointer() && value->GetObjectPointer()->IsClosure())
					{
						return value;
					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a closure.", GetLine()));
					}
				}));

			MidoriValue& closure_value = *m_last_result.value();
			Traceable* new_closure = RuntimeAllocateObject(Traceable::Closure(closure_value.GetObjectPointer()->GetClosure()));
			Traceable* old_closure = closure_value.GetObjectPointer();
			Traceable::Closure& new_closure_ref = new_closure->GetClosure();
			Traceable::Closure& old_closure_ref = old_closure->GetClosure();
			std::vector<Traceable*>& new_captured_variables = new_closure_ref.m_cell_values;
			std::vector<Traceable*>& old_captured_variables = old_closure_ref.m_cell_values;

			if (!m_closure_stack.empty())
			{
				new_captured_variables = m_closure_stack.top()->m_cell_values;
				old_captured_variables = m_closure_stack.top()->m_cell_values;
				count -= static_cast<int>(m_closure_stack.top()->m_cell_values.size());
			}

			for (StackPointer<MidoriValue, VALUE_STACK_MAX> it = m_base_pointer; it < m_base_pointer + count; ++it)
			{
				if (it->IsObjectPointer() && it->GetObjectPointer()->IsCellValue())
				{
					new_captured_variables.emplace_back(it->GetObjectPointer());
					old_captured_variables.emplace_back(it->GetObjectPointer());
				}
				else
				{
					Traceable* cell_value = RuntimeAllocateObject(Traceable::CellValue(std::move(*it)));
					*it = MidoriValue(cell_value);
					new_captured_variables.emplace_back(cell_value);
					old_captured_variables.emplace_back(cell_value);
				}
			}

			EXECUTE_OR_ABORT(Push(MidoriValue(new_closure)));
			break;
		}
		case OpCode::GET_GLOBAL:
		{
			const std::string& name = ReadGlobalVariable();
			GlobalVariables::iterator it = m_global_vars.find(name);
			EXECUTE_OR_ABORT(Push(MidoriValue(m_global_vars[name])));
			break;
		}
		case OpCode::SET_GLOBAL:
		{
			const std::string& name = ReadGlobalVariable();
			GlobalVariables::iterator it = m_global_vars.find(name);
			MidoriValue& var = m_global_vars[name];
			EXECUTE_OR_ABORT(Bind(Duplicate(), [&var, this](MidoriValue*) -> MidoriResult::InterpreterResult
				{
					return Bind(Pop(), [&var, this](MidoriValue* value_copy) -> MidoriResult::InterpreterResult
						{
							var = std::move(*value_copy);
							return &var;
						});
				}));
			break;
		}
		case OpCode::GET_LOCAL:
		{
			int offset = static_cast<int>(ReadByte());
			MidoriValue value_copy = *(m_base_pointer + offset);
			EXECUTE_OR_ABORT(Push(std::move(value_copy)));
			break;
		}
		case OpCode::SET_LOCAL:
		{
			int offset = static_cast<int>(ReadByte());
			MidoriValue& var = *(m_base_pointer + offset);
			EXECUTE_OR_ABORT(Bind(Duplicate(), [&var, this](MidoriValue*) -> MidoriResult::InterpreterResult
				{
					return Bind(Pop(), [&var, this](MidoriValue* value_copy) -> MidoriResult::InterpreterResult
						{
							if (var.IsObjectPointer() && var.GetObjectPointer()->IsCellValue())
							{
								var.GetObjectPointer()->GetCellValue().m_value = std::move(*value_copy);
							}
							else
							{
								var = std::move(*value_copy);
							}

							return &var;
						});
				}));
			break;
		}
		case OpCode::GET_CELL:
		{
			int offset = static_cast<int>(ReadByte());
			MidoriValue value_copy = m_closure_stack.top()->m_cell_values[offset]->GetCellValue().m_value;
			EXECUTE_OR_ABORT(Push(std::move(value_copy)));
			break;
		}
		case OpCode::SET_CELL:
		{
			int offset = static_cast<int>(ReadByte());
			EXECUTE_OR_ABORT(Bind(Duplicate(), [offset, this](MidoriValue*) -> MidoriResult::InterpreterResult
				{
					return Bind(Pop(), [offset, this](MidoriValue* value_copy) -> MidoriResult::InterpreterResult
						{
							MidoriValue& var = m_closure_stack.top()->m_cell_values[offset]->GetCellValue().m_value;
							var = std::move(*value_copy);
							return &var;
						});
				}));
			break;
		}
		case OpCode::POP:
		{
			EXECUTE_OR_ABORT(Pop());
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
			if (m_call_stack_pointer == m_call_stack.begin())
			{
				return std::unexpected<std::string>(GenerateRuntimeError("Call stack underflow.", GetLine()));
			}
			CallFrame& top_frame = *(--m_call_stack_pointer);
			m_closure_stack.pop();

			EXECUTE_OR_ABORT(Pop());
			MidoriValue& return_val = *m_last_result.value();

			m_current_bytecode = top_frame.m_return_module;
			m_base_pointer = top_frame.m_return_bp;
			m_instruction_pointer = top_frame.m_return_ip;
			m_value_stack_pointer = top_frame.m_return_sp;

			EXECUTE_OR_ABORT(Push(std::move(return_val)));
			break;
		}
		case OpCode::HALT:
		{
			return std::unexpected<std::string>(GenerateRuntimeError("Program halted.", GetLine()));
		}
		default:
		{
			return std::unexpected<std::string>(GenerateRuntimeError("Unknown instruction: " + std::to_string(static_cast<int>(instruction)), GetLine()));
		}
		}
	}

	return m_last_result;
}
