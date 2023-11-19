#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"

#include <cmath>
#include <algorithm>
#include <numeric>

#ifdef DEBUG
#include <iostream>
#endif

VirtualMachine::VirtualMachine(MidoriResult::ExecutableModule&& executable_module) : m_executable_module(std::move(executable_module)), m_garbage_collector(std::move(m_executable_module.m_constant_roots)), m_current_bytecode(&m_executable_module.m_modules[0]), m_instruction_pointer(m_current_bytecode->cbegin())
{
	m_dispatch_table[static_cast<size_t>(OpCode::CONSTANT)] = [this]() { Push(ReadConstant(OpCode::CONSTANT));  };
	m_dispatch_table[static_cast<size_t>(OpCode::CONSTANT_LONG)] = [this]() { Push(ReadConstant(OpCode::CONSTANT_LONG));  };
	m_dispatch_table[static_cast<size_t>(OpCode::CONSTANT_LONG_LONG)] = [this]() { Push(ReadConstant(OpCode::CONSTANT_LONG_LONG));  };
	m_dispatch_table[static_cast<size_t>(OpCode::UNIT)] = [this]() { Push(MidoriValue());  };
	m_dispatch_table[static_cast<size_t>(OpCode::TRUE)] = [this]() { Push(MidoriValue(true));  };
	m_dispatch_table[static_cast<size_t>(OpCode::FALSE)] = [this]() { Push(MidoriValue(false));  };
	m_dispatch_table[static_cast<size_t>(OpCode::CREATE_ARRAY)] = [this]()
		{
			int count = ReadThreeBytes();
			std::vector<MidoriValue> arr(count);

			std::for_each(arr.rbegin(), arr.rend(), [this](MidoriValue& value) -> void
				{
					value = std::move(Pop());
				});
			Push(CollectGarbageThenAllocateObject(std::move(arr)));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::GET_ARRAY)] = [this]()
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
		};
	m_dispatch_table[static_cast<size_t>(OpCode::SET_ARRAY)] = [this]()
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
		};
	m_dispatch_table[static_cast<size_t>(OpCode::LEFT_SHIFT)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() << right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::RIGHT_SHIFT)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() >> right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::BITWISE_AND)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() & right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::BITWISE_OR)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() | right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::BITWISE_XOR)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() ^ right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::BITWISE_NOT)] = [this]()
		{
			const MidoriValue& right = Pop();
			Push(MidoriValue(~right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::ADD_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() + right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::SUBTRACT_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() - right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::MULTIPLY_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() * right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::DIVIDE_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() / right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::MODULO_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(std::fmod(left.GetFraction(), right.GetFraction())));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::ADD_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() + right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::SUBTRACT_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() - right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::MULTIPLY_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() * right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::DIVIDE_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() / right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::MODULO_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() % right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::CONCAT_ARRAY)] = [this]()
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
		};
	m_dispatch_table[static_cast<size_t>(OpCode::CONCAT_TEXT)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();

			const MidoriTraceable::MidoriText& left_value_string_ref = left.GetObjectPointer()->GetText();
			const MidoriTraceable::MidoriText& right_value_string_ref = right.GetObjectPointer()->GetText();

			MidoriTraceable::MidoriText new_value_string = left_value_string_ref + right_value_string_ref;

			Push(CollectGarbageThenAllocateObject(std::move(new_value_string)));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::EQUAL_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() == right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::NOT_EQUAL_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() != right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::GREATER_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() > right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::GREATER_EQUAL_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() >= right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::LESS_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() < right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::LESS_EQUAL_FRACTION)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetFraction() <= right.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::EQUAL_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() == right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::NOT_EQUAL_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() != right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::GREATER_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() > right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::GREATER_EQUAL_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() >= right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::LESS_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() < right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::LESS_EQUAL_INTEGER)] = [this]()
		{
			const MidoriValue& right = Pop();
			const MidoriValue& left = Pop();
			Push(MidoriValue(left.GetInteger() <= right.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::NOT)] = [this]()
		{
			const MidoriValue& value = Pop();
			Push(MidoriValue(!value.GetBool()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::NEGATE_FRACTION)] = [this]()
		{
			const MidoriValue& value = Pop();
			Push(MidoriValue(-value.GetFraction()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::NEGATE_INTEGER)] = [this]()
		{
			const MidoriValue& value = Pop();
			Push(MidoriValue(-value.GetInteger()));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::JUMP_IF_FALSE)] = [this]()
		{
			if (Duplicate() == false)
			{
				return;
			}

			MidoriValue& value = Pop();

			int offset = ReadShort();
			if (!value.GetBool())
			{
				m_instruction_pointer += offset;
			}
		};
	m_dispatch_table[static_cast<size_t>(OpCode::JUMP_IF_TRUE)] = [this]()
		{
			if (Duplicate() == false)
			{
				return;
			}

			MidoriValue& value = Pop();

			int offset = ReadShort();
			if (value.GetBool())
			{
				m_instruction_pointer += offset;
			}
		};
	m_dispatch_table[static_cast<size_t>(OpCode::JUMP)] = [this]()
		{
			int offset = ReadShort();
			m_instruction_pointer += offset;
		};
	m_dispatch_table[static_cast<size_t>(OpCode::JUMP_BACK)] = [this]()
		{
			int offset = ReadShort();
			m_instruction_pointer -= offset;
		};
	m_dispatch_table[static_cast<size_t>(OpCode::CALL)] = [this]()
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
					return;
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
		};
	m_dispatch_table[static_cast<size_t>(OpCode::CREATE_CLOSURE)] = [this]()
		{
			int captured_count = static_cast<int>(ReadByte());
			int arity = static_cast<int>(ReadByte());
			int module_index = static_cast<int>(ReadByte());

			if (Push(MidoriValue(AllocateObject(MidoriTraceable::Closure(std::vector<MidoriTraceable*>(), module_index, arity)))) == false)
			{
				return;
			}

			if (captured_count == 0)
			{
				return;
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
		};
	m_dispatch_table[static_cast<size_t>(OpCode::DEFINE_GLOBAL)] = [this]()
		{
			MidoriValue& value = Pop();
			const std::string& name = ReadGlobalVariable();
			MidoriValue& var = m_global_vars[name];
			var = std::move(value);
		};
	m_dispatch_table[static_cast<size_t>(OpCode::GET_GLOBAL)] = [this]()
		{
			const std::string& name = ReadGlobalVariable();
			Push(MidoriValue(m_global_vars[name]));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::SET_GLOBAL)] = [this]()
		{
			if (Duplicate() == false)
			{
				return;
			}
			const std::string& name = ReadGlobalVariable();
			MidoriValue& var = m_global_vars[name];

			MidoriValue& value_copy = Pop();
			var = std::move(value_copy);
		};
	m_dispatch_table[static_cast<size_t>(OpCode::GET_LOCAL)] = [this]()
		{
			int offset = static_cast<int>(ReadByte());
			MidoriValue value_copy = *(m_base_pointer + offset);
			Push(std::move(value_copy));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::SET_LOCAL)] = [this]()
		{
			if (Duplicate() == false)
			{
				return;
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
		};
	m_dispatch_table[static_cast<size_t>(OpCode::GET_CELL)] = [this]()
		{
			int offset = static_cast<int>(ReadByte());
			MidoriValue value_copy = m_closure_stack.top()->m_cell_values[offset]->GetCellValue();
			Push(std::move(value_copy));
		};
	m_dispatch_table[static_cast<size_t>(OpCode::SET_CELL)] = [this]()
		{
			if (Duplicate() == false)
			{
				return;
			}

			int offset = static_cast<int>(ReadByte());
			MidoriValue& var = m_closure_stack.top()->m_cell_values[offset]->GetCellValue();
			var = std::move(Pop());
		};
	m_dispatch_table[static_cast<size_t>(OpCode::POP)] = [this]()
		{
			--m_value_stack_pointer;
		};
	m_dispatch_table[static_cast<size_t>(OpCode::POP_MULTIPLE)] = [this]()
		{
			m_value_stack_pointer -= static_cast<int>(ReadByte());
		};
	m_dispatch_table[static_cast<size_t>(OpCode::RETURN)] = [this]()
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
		};
	m_dispatch_table[static_cast<size_t>(OpCode::HALT)] = [this]()
		{
			DisplayRuntimeError(GenerateRuntimeError("Program halted.", GetLine()));
		};
}

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
		m_dispatch_table[static_cast<size_t>(instruction)]();
	}

	return;
}
