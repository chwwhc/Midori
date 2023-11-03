#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"

#include <cmath>
#include <algorithm>
#include <numeric>

#ifdef DEBUG
#include <iostream>
#endif

#define EXECUTE_OR_ABORT(operation)							  \
    MidoriResult::InterpreterResult result = operation;       \
    if (!result.has_value())								  \
        return result

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
										return Bind(Pop(), [&op, this](MidoriValue* right) -> MidoriResult::InterpreterResult
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

Traceable::GarbageCollectionRoots VirtualMachine::GetGlobalTableGarbageCollectionRoots()
{
	Traceable::GarbageCollectionRoots roots;

	std::for_each(m_global_vars.cbegin(), m_global_vars.cend(), [&roots](const GlobalVariables::value_type& pair) -> void
		{
			if (pair.second.IsObjectPointer())
			{
				roots.emplace(static_cast<Traceable*>(pair.second.GetObjectPointer()));
			}
		});

	return roots;
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
	Traceable::GarbageCollectionRoots stack_roots = GetValueStackGarbageCollectionRoots();
	Traceable::GarbageCollectionRoots global_roots = GetGlobalTableGarbageCollectionRoots();

	stack_roots.insert(global_roots.begin(), global_roots.end());
	return stack_roots;
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
		std::for_each(m_base_pointer, m_value_stack_pointer, [](MidoriValue& value) -> void
			{
				std::cout << "[ " << value.ToString() << " ]";
			});
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
			EXECUTE_OR_ABORT(Push(ReadConstant(instruction)));
			break;
		}
		case OpCode::UNIT:
		{
			EXECUTE_OR_ABORT(Push(MidoriValue()));
			break;
		}
		case OpCode::TRUE:
		{
			EXECUTE_OR_ABORT(Push(MidoriValue(true)));
			break;
		}
		case OpCode::FALSE:
		{
			EXECUTE_OR_ABORT(Push(MidoriValue(false)));
			break;
		}
		case OpCode::CREATE_ARRAY:
		{
			int count = ReadThreeBytes();
			std::vector<MidoriValue> arr(count);

			std::for_each(arr.rbegin(), arr.rend(), [this](MidoriValue& value) -> void
				{
					value = std::move(*Pop().value());
				});
			Push(CollectGarbageThenAllocateObject(std::move(arr)));
			break;
		}
		case OpCode::GET_ARRAY:
		{
			int num_indices = static_cast<int>(ReadByte());
			std::vector<MidoriValue> indices(num_indices);

			std::for_each(indices.rbegin(), indices.rend(), [this](MidoriValue& value) -> void
				{
					value = std::move(*Pop().value());
				});

			MidoriValue& arr = *Pop().value();
			EXECUTE_OR_ABORT(CheckIsArray(&arr));
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
					Push(std::move(next_val));
				}
			}
			break;
		}
		case OpCode::SET_ARRAY:
		{
			int num_indices = static_cast<int>(ReadByte());
			MidoriValue& value_to_set = *Pop().value();
			std::vector<MidoriValue> indices(num_indices);

			std::for_each(indices.rbegin(), indices.rend(), [this](MidoriValue& value) -> void
				{
					value = std::move(*Pop().value());
				});

			MidoriValue& arr = *Pop().value();
			EXECUTE_OR_ABORT(CheckIsArray(&arr));
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

			Push(std::move(value_to_set));
			break;
		}
		case OpCode::RESERVE_ARRAY:
		{
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					return Bind(CheckIsNumber(value), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
						{
							return Bind(CheckArrayMaxLength(value), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
								{
									return Push(CollectGarbageThenAllocateObject(std::vector<MidoriValue>(static_cast<size_t>(value->GetNumber()))));
								});
						});
				}));
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
			EXECUTE_OR_ABORT(BinaryOperation([this](const MidoriValue& left, const MidoriValue& right) -> MidoriValue
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

						return CollectGarbageThenAllocateObject(std::move(new_value_vector));
					}
					else {
						const std::string& left_value_string_ref = left_value->GetString();
						const std::string& right_value_string_ref = right_value->GetString();

						std::string new_value_string = left_value_string_ref + right_value_string_ref;

						return CollectGarbageThenAllocateObject(std::move(new_value_string));
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
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					int offset = ReadShort();
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
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					int offset = ReadShort();
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
						const MidoriValue& callable = *value;
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
							m_current_bytecode = &m_executable_module.m_modules[closure.m_module_index];
							m_instruction_pointer = m_current_bytecode->cbegin();
							m_base_pointer = m_value_stack_pointer - arity;


							std::for_each_n(m_base_pointer, std::distance(m_base_pointer, m_value_stack_pointer), [](MidoriValue& value) -> void
								{
									if (value.IsObjectPointer() && value.GetObjectPointer()->IsCellValue())
									{
										value = value.GetObjectPointer()->GetCellValue();
									}
								});
						}

						return value;

					}
					else
					{
						return std::unexpected<std::string>(GenerateRuntimeError("Operand must be a callable.", GetLine()));
					}
				}));
			break;
		}
		case OpCode::CREATE_CLOSURE:
		{
			int captured_count = static_cast<int>(ReadByte());
			int arity = static_cast<int>(ReadByte());
			int module_index = static_cast<int>(ReadByte());

			EXECUTE_OR_ABORT(Push(MidoriValue(AllocateObject(Traceable::Closure(std::vector<Traceable*>(), module_index, arity)))));

			if (captured_count == 0)
			{
				break;
			}

			std::vector<Traceable*>& captured_variables = std::prev(m_value_stack_pointer)->GetObjectPointer()->GetClosure().m_cell_values;

			if (!m_closure_stack.empty())
			{
				captured_variables = m_closure_stack.top()->m_cell_values;
				captured_count -= static_cast<int>(m_closure_stack.top()->m_cell_values.size());
			}

			for (StackPointer<MidoriValue, VALUE_STACK_MAX> it = m_base_pointer; it < m_base_pointer + captured_count; ++it)
			{
				if (it->IsObjectPointer() && it->GetObjectPointer()->IsCellValue())
				{
					captured_variables.emplace_back(it->GetObjectPointer());
				}
				else
				{
					Traceable* cell_value = AllocateObject(Traceable::CellValue(std::move(*it)));
					*it = MidoriValue(cell_value);
					captured_variables.emplace_back(cell_value);
				}
			}

			break;
		}
		case OpCode::DEFINE_GLOBAL:
		{
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value_copy) -> MidoriResult::InterpreterResult
				{
					const std::string& name = ReadGlobalVariable();
					GlobalVariables::iterator it = m_global_vars.find(name);
					MidoriValue& var = m_global_vars[name];
					var = std::move(*value_copy);
					return &var;
				}));
			break;
		}
		case OpCode::GET_GLOBAL:
		{
			const std::string& name = ReadGlobalVariable();
			EXECUTE_OR_ABORT(Push(MidoriValue(m_global_vars[name])));
			break;
		}
		case OpCode::SET_GLOBAL:
		{
			EXECUTE_OR_ABORT(Bind(Duplicate(), [this](MidoriValue*) -> MidoriResult::InterpreterResult
				{
					const std::string& name = ReadGlobalVariable();
					MidoriValue& var = m_global_vars[name];
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
			EXECUTE_OR_ABORT(Bind(Duplicate(), [this](MidoriValue*) -> MidoriResult::InterpreterResult
				{
					int offset = static_cast<int>(ReadByte());
					MidoriValue& var = *(m_base_pointer + offset);
					return Bind(Pop(), [&var, this](MidoriValue* value_copy) -> MidoriResult::InterpreterResult
						{
							if (var.IsObjectPointer() && var.GetObjectPointer()->IsCellValue())
							{
								var.GetObjectPointer()->GetCellValue() = std::move(*value_copy);
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
			MidoriValue value_copy = m_closure_stack.top()->m_cell_values[offset]->GetCellValue();
			EXECUTE_OR_ABORT(Push(std::move(value_copy)));
			break;
		}
		case OpCode::SET_CELL:
		{
			EXECUTE_OR_ABORT(Bind(Duplicate(), [this](MidoriValue*) -> MidoriResult::InterpreterResult
				{
					int offset = static_cast<int>(ReadByte());
					return Bind(Pop(), [offset, this](MidoriValue* value_copy) -> MidoriResult::InterpreterResult
						{
							MidoriValue& var = m_closure_stack.top()->m_cell_values[offset]->GetCellValue();
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
			m_value_stack_pointer -= static_cast<int>(ReadByte());
			break;
		}
		case OpCode::RETURN:
		{
			EXECUTE_OR_ABORT(Bind(Pop(), [this](MidoriValue* value) -> MidoriResult::InterpreterResult
				{
					CallFrame& top_frame = *(--m_call_stack_pointer);
					m_closure_stack.pop();
					MidoriValue& return_val = *value;

					m_current_bytecode = top_frame.m_return_module;
					m_base_pointer = top_frame.m_return_bp;
					m_instruction_pointer = top_frame.m_return_ip;
					m_value_stack_pointer = top_frame.m_return_sp;

					return Push(std::move(return_val));
				}));
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

	return std::addressof(*m_value_stack_pointer);
}
