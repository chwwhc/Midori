#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"

#include <cmath>
#include <algorithm>
#include <numeric>

#ifdef DEBUG
#include <iostream>
#endif

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetGlobalTableGarbageCollectionRoots()
{
	MidoriTraceable::GarbageCollectionRoots roots;

	std::for_each(m_global_vars.cbegin(), m_global_vars.cend(), [&roots](const GlobalVariables::value_type& pair) -> void
		{
			if (pair.second.IsObjectPointer())
			{
				roots.emplace(static_cast<MidoriTraceable*>(pair.second.GetObjectPointer()));
			}
		});

	return roots;
}

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetValueStackGarbageCollectionRoots()
{
	MidoriTraceable::GarbageCollectionRoots roots;

	std::for_each_n(m_value_stack->begin(), std::distance(m_value_stack->begin(), m_value_stack_pointer), [&roots](MidoriValue& value) -> void
		{
			if (value.IsObjectPointer())
			{
				roots.emplace(static_cast<MidoriTraceable*>(value.GetObjectPointer()));
			}
		});

	return roots;
}

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetGarbageCollectionRoots()
{
	MidoriTraceable::GarbageCollectionRoots stack_roots = GetValueStackGarbageCollectionRoots();
	MidoriTraceable::GarbageCollectionRoots global_roots = GetGlobalTableGarbageCollectionRoots();

	stack_roots.insert(global_roots.begin(), global_roots.end());
	return stack_roots;
}

void VirtualMachine::CollectGarbage()
{
	if (MidoriTraceable::s_total_bytes_allocated - MidoriTraceable::s_static_bytes_allocated < s_garbage_collection_threshold)
	{
		return;
	}

	MidoriTraceable::GarbageCollectionRoots roots = GetGarbageCollectionRoots();
	if (roots.empty())
	{
		return;
	}

#ifdef DEBUG
	std::cout << "\nBefore garbage collection:";
	MidoriTraceable::PrintMemoryTelemetry();
#endif
	m_garbage_collector.ReclaimMemory(std::move(roots));
#ifdef DEBUG
	std::cout << "\nAfter garbage collection:";
	MidoriTraceable::PrintMemoryTelemetry();
#endif
}

void VirtualMachine::Execute()
{
	NativeFunction::InitializeNativeFunctions(*this);

	while ((m_instruction_pointer != m_current_bytecode->cend()) && (m_error == false))
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
			Push(ReadConstant(instruction));
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

			std::for_each(arr.rbegin(), arr.rend(), [this](MidoriValue& value) -> void
				{
					value = std::move(Pop());
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
					value = std::move(Pop());
				});

			MidoriValue& arr = Pop();
			std::vector<MidoriValue>& arr_ref = arr.GetObjectPointer()->GetArray();
			MidoriValue::MidoriInteger arr_size = static_cast<MidoriValue::MidoriInteger>(arr_ref.size());

			for (MidoriValue& index : indices)
			{
				if (CheckIndexBounds(&index, arr_size) == false)
				{
					break;
				}

				MidoriValue& next_val = arr_ref[static_cast<size_t>(index.GetInteger())];

				if (&index != &indices.back())
				{
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
			MidoriValue& value_to_set = Pop();
			std::vector<MidoriValue> indices(num_indices);

			std::for_each(indices.rbegin(), indices.rend(), [this](MidoriValue& value) -> void
				{
					value = std::move(Pop());
				});

			MidoriValue& arr = Pop();
			std::vector<MidoriValue>& arr_ref = arr.GetObjectPointer()->GetArray();
			MidoriValue::MidoriInteger arr_size = static_cast<MidoriValue::MidoriInteger>(arr_ref.size());

			for (MidoriValue& index : indices)
			{
				if (CheckIndexBounds(&index, arr_size) == false)
				{
					break;
				}

				size_t i = static_cast<size_t>(index.GetInteger());

				if (&index != &indices.back())
				{
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
		case OpCode::LEFT_SHIFT:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(static_cast<double>(static_cast<int>(left.GetFraction()) << static_cast<int>(right.GetFraction()))));
			break;
		}
		case OpCode::RIGHT_SHIFT:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(static_cast<double>(static_cast<int>(left.GetFraction()) >> static_cast<int>(right.GetFraction()))));
			break;
		}
		case OpCode::BITWISE_AND:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(static_cast<double>(static_cast<int>(left.GetFraction()) & static_cast<int>(right.GetFraction()))));
			break;
		}
		case OpCode::BITWISE_OR:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(static_cast<double>(static_cast<int>(left.GetFraction()) | static_cast<int>(right.GetFraction()))));
			break;
		}
		case OpCode::BITWISE_XOR:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(static_cast<double>(static_cast<int>(left.GetFraction()) ^ static_cast<int>(right.GetFraction()))));
			break;
		}
		case OpCode::ADD_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() + right.GetFraction()));
			break;
		}
		case OpCode::SUBTRACT_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() - right.GetFraction()));
			break;
		}
		case OpCode::MULTIPLY_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() * right.GetFraction()));
			break;
		}
		case OpCode::DIVIDE_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() / right.GetFraction()));
			break;
		}
		case OpCode::MODULO_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(std::fmod(left.GetFraction(), right.GetFraction())));
			break;
		}
		case OpCode::ADD_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() + right.GetInteger()));
			break;
		}
		case OpCode::SUBTRACT_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() - right.GetInteger()));
			break;
		}
		case OpCode::MULTIPLY_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() * right.GetInteger()));
			break;
		}
		case OpCode::DIVIDE_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() / right.GetInteger()));
			break;
		}
		case OpCode::MODULO_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() % right.GetInteger()));
			break;
		}
		case OpCode::CONCAT_ARRAY:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();

			const MidoriTraceable::MidoriArray& left_value_vector_ref = left.GetObjectPointer()->GetArray();
			const MidoriTraceable::MidoriArray& right_value_vector_ref = right.GetObjectPointer()->GetArray();

			MidoriTraceable::MidoriArray new_value_vector(left_value_vector_ref);
			new_value_vector.insert(new_value_vector.end(),
				right_value_vector_ref.begin(),
				right_value_vector_ref.end());

			Push(CollectGarbageThenAllocateObject(std::move(new_value_vector)));
			break;
		}
		case OpCode::CONCAT_TEXT:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();

			const MidoriTraceable::MidoriText& left_value_string_ref = left.GetObjectPointer()->GetText();
			const MidoriTraceable::MidoriText& right_value_string_ref = right.GetObjectPointer()->GetText();

			MidoriTraceable::MidoriText new_value_string = left_value_string_ref + right_value_string_ref;

			Push(CollectGarbageThenAllocateObject(std::move(new_value_string)));
			break;
		}
		case OpCode::EQUAL_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() == right.GetFraction()));
			break;
		}
		case OpCode::NOT_EQUAL_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() != right.GetFraction()));
			break;
		}
		case OpCode::GREATER_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() > right.GetFraction()));
			break;
		}
		case OpCode::GREATER_EQUAL_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() >= right.GetFraction()));
			break;
		}
		case OpCode::LESS_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() < right.GetFraction()));
			break;
		}
		case OpCode::LESS_EQUAL_FRACTION:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() <= right.GetFraction()));
			break;
		}
		case OpCode::EQUAL_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() == right.GetInteger()));
			break;
		}
		case OpCode::NOT_EQUAL_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() != right.GetInteger()));
			break;
		}
		case OpCode::GREATER_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() > right.GetInteger()));
			break;
		}
		case OpCode::GREATER_EQUAL_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() >= right.GetInteger()));
			break;
		}
		case OpCode::LESS_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() < right.GetInteger()));
			break;
		}
		case OpCode::LESS_EQUAL_INTEGER:
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() <= right.GetInteger()));
			break;
		}
		case OpCode::NOT:
		{
			const MidoriValue& value = Pop();
			Push(MidoriValue(!value.GetBool()));
			break;
		}
		case OpCode::NEGATE_FRACTION:
		{
			const MidoriValue& value = Pop();
			Push(MidoriValue(-value.GetFraction()));
			break;
		}
		case OpCode::NEGATE_INTEGER:
		{
			const MidoriValue& value = Pop();
			Push(MidoriValue(-value.GetInteger()));
			break;
		}
		case OpCode::JUMP_IF_FALSE:
		{
			if (Duplicate() == false)
			{
				break;
			}

			MidoriValue& value = Pop();

			int offset = ReadShort();
			if (!value.GetBool())
			{
				m_instruction_pointer += offset;
			}

			break;
		}
		case OpCode::JUMP_IF_TRUE:
		{
			if (Duplicate() == false)
			{
				break;
			}

			MidoriValue& value = Pop();

			int offset = ReadShort();
			if (value.GetBool())
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
			const MidoriValue& callable = Pop();
			int arity = static_cast<int>(ReadByte());

			// Native function
			if (callable.IsNativeFunction())
			{
				MidoriValue::NativeFunction& native_function = callable.GetNativeFunction();

				native_function.m_cpp_function();
			}
			else if (callable.IsObjectPointer() && callable.GetObjectPointer()->IsClosure())
			{
				MidoriTraceable::Closure& closure = callable.GetObjectPointer()->GetClosure();

				m_closure_stack.emplace(&closure);

				if (std::next(m_call_stack_pointer) == m_call_stack->end())
				{
					DisplayRuntimeError(GenerateRuntimeError("Call stack overflow.", GetLine()));
					break;
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

			break;
		}
		case OpCode::CREATE_CLOSURE:
		{
			int captured_count = static_cast<int>(ReadByte());
			int arity = static_cast<int>(ReadByte());
			int module_index = static_cast<int>(ReadByte());

			if (Push(MidoriValue(AllocateObject(MidoriTraceable::Closure(std::vector<MidoriTraceable*>(), module_index, arity)))) == false)
			{
				break;
			}

			if (captured_count == 0)
			{
				break;
			}

			std::vector<MidoriTraceable*>& captured_variables = std::prev(m_value_stack_pointer)->GetObjectPointer()->GetClosure().m_cell_values;

			if (!m_closure_stack.empty())
			{
				captured_variables = m_closure_stack.top()->m_cell_values;
				captured_count -= static_cast<int>(m_closure_stack.top()->m_cell_values.size());
			}

			for (StackPointer<MidoriValue, s_value_stack_max> it = m_base_pointer; it < m_base_pointer + captured_count; ++it)
			{
				if (it->IsObjectPointer() && it->GetObjectPointer()->IsCellValue())
				{
					captured_variables.emplace_back(it->GetObjectPointer());
				}
				else
				{
					MidoriTraceable* cell_value = AllocateObject(MidoriTraceable::CellValue(std::move(*it)));
					*it = MidoriValue(cell_value);
					captured_variables.emplace_back(cell_value);
				}
			}

			break;
		}
		case OpCode::DEFINE_GLOBAL:
		{
			MidoriValue& value = Pop();
			const std::string& name = ReadGlobalVariable();

			MidoriValue& var = m_global_vars[name];
			var = std::move(value);

			break;
		}
		case OpCode::GET_GLOBAL:
		{
			const std::string& name = ReadGlobalVariable();
			Push(MidoriValue(m_global_vars[name]));
			break;
		}
		case OpCode::SET_GLOBAL:
		{
			if (Duplicate() == false)
			{
				break;
			}
			const std::string& name = ReadGlobalVariable();
			MidoriValue& var = m_global_vars[name];

			MidoriValue& value_copy = Pop();
			var = std::move(value_copy);

			break;
		}
		case OpCode::GET_LOCAL:
		{
			int offset = static_cast<int>(ReadByte());
			MidoriValue value_copy = *(m_base_pointer + offset);
			Push(std::move(value_copy));
			break;
		}
		case OpCode::SET_LOCAL:
		{
			if (Duplicate() == false)
			{
				break;
			}

			int offset = static_cast<int>(ReadByte());
			MidoriValue& var = *(m_base_pointer + offset);

			MidoriValue& value_copy = Pop();
			if (var.IsObjectPointer() && var.GetObjectPointer()->IsCellValue())
			{
				var.GetObjectPointer()->GetCellValue() = std::move(value_copy);
			}
			else
			{
				var = std::move(value_copy);
			}
			break;
		}
		case OpCode::GET_CELL:
		{
			int offset = static_cast<int>(ReadByte());
			MidoriValue value_copy = m_closure_stack.top()->m_cell_values[offset]->GetCellValue();
			Push(std::move(value_copy));
			break;
		}
		case OpCode::SET_CELL:
		{
			if (Duplicate() == false)
			{
				break;
			}

			int offset = static_cast<int>(ReadByte());
			MidoriValue& var = m_closure_stack.top()->m_cell_values[offset]->GetCellValue();
			var = std::move(Pop());

			break;
		}
		case OpCode::POP:
		{
			Pop();
			break;
		}
		case OpCode::POP_MULTIPLE:
		{
			m_value_stack_pointer -= static_cast<int>(ReadByte());
			break;
		}
		case OpCode::RETURN:
		{
			MidoriValue& value = Pop();

			CallFrame& top_frame = *(--m_call_stack_pointer);
			m_closure_stack.pop();
			MidoriValue& return_val = value;

			m_current_bytecode = top_frame.m_return_module;
			m_base_pointer = top_frame.m_return_bp;
			m_instruction_pointer = top_frame.m_return_ip;
			m_value_stack_pointer = top_frame.m_return_sp;

			Push(std::move(return_val));

			break;
		}
		case OpCode::HALT:
		{
			DisplayRuntimeError(GenerateRuntimeError("Program halted.", GetLine()));
			break;
		}
		default:
		{
			DisplayRuntimeError(GenerateRuntimeError("Unknown instruction: " + std::to_string(static_cast<int>(instruction)), GetLine()));
			break;
		}
		}
	}

	return;
}
