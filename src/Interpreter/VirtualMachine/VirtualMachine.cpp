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
	try
	{
#ifdef _WIN32
		m_library_handle = LoadLibrary("./MidoriStdLib.dll");
#else
		m_library_handle = dlopen("./libMidoriStdLib.so", RTLD_LAZY);
#endif

		if (m_library_handle == NULL)
		{
#ifdef _WIN32
			FreeLibrary(m_library_handle);
#else
			dlclose(m_library_handle);
#endif
			throw InterpreterException("Failed to load the standard library.");
		}

		while (true)
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
			case OpCode::OP_UNIT:
			{
				Push(MidoriValue());
				break;
			}
			case OpCode::OP_TRUE:
			{
				Push(MidoriValue(true));
				break;
			}
			case OpCode::OP_FALSE:
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
						value = Pop();
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
						value = Pop();
					});

				MidoriValue& arr = Pop();
				std::vector<MidoriValue>& arr_ref = arr.GetObjectPointer()->GetArray();
				MidoriValue::MidoriInteger arr_size = static_cast<MidoriValue::MidoriInteger>(arr_ref.size());

				for (MidoriValue& index : indices)
				{
					CheckIndexBounds(index, arr_size);

					MidoriValue& next_val = arr_ref[static_cast<size_t>(index.GetInteger())];

					if (&index != &indices.back())
					{
						arr_ref = next_val.GetObjectPointer()->GetArray();
					}
					else
					{
						Push(next_val);
					}
				}
				break;
			}
			case OpCode::SET_ARRAY:
			{
				int num_indices = static_cast<int>(ReadByte());
				const MidoriValue& value_to_set = Pop();
				std::vector<MidoriValue> indices(num_indices);

				std::for_each(indices.rbegin(), indices.rend(), [this](MidoriValue& value) -> void
					{
						value = Pop();
					});

				MidoriValue& arr = Pop();
				std::vector<MidoriValue>& arr_ref = arr.GetObjectPointer()->GetArray();
				MidoriValue::MidoriInteger arr_size = static_cast<MidoriValue::MidoriInteger>(arr_ref.size());

				for (MidoriValue& index : indices)
				{
					CheckIndexBounds(index, arr_size);

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

				Push(value_to_set);
				break;
			}
			case OpCode::CAST_TO_FRACTION:
			{
				const MidoriValue& value = Pop();

				if (value.IsBool())
				{
					Push(MidoriValue(static_cast<MidoriValue::MidoriFraction>(value.GetBool())));
				}
				else if (value.IsInteger())
				{
					Push(MidoriValue(static_cast<MidoriValue::MidoriFraction>(value.GetInteger())));
				}
				else if (value.IsFraction())
				{
					Push(value);
				}
				else if (value.IsObjectPointer() && value.GetObjectPointer()->IsText())
				{
					MidoriValue::MidoriFraction fraction = std::stod(value.GetObjectPointer()->GetText());
					Push(MidoriValue(fraction));
				}
				else if (value.IsUnit())
				{
					Push(MidoriValue(static_cast<MidoriValue::MidoriFraction>(0.0)));
				}
				else
				{
					// unreachable
					throw InterpreterException(GenerateRuntimeError("Cannot cast to fraction.", GetLine()));
				}

				break;
			}
			case OpCode::CAST_TO_INTEGER:
			{
				const MidoriValue& value = Pop();

				if (value.IsBool())
				{
					Push(MidoriValue(static_cast<MidoriValue::MidoriInteger>(value.GetBool())));
				}
				else if (value.IsInteger())
				{
					Push(value);
				}
				else if (value.IsFraction())
				{
					Push(MidoriValue(static_cast<MidoriValue::MidoriInteger>(value.GetFraction())));
				}
				else if (value.IsObjectPointer() && value.GetObjectPointer()->IsText())
				{
					MidoriValue::MidoriInteger integer = std::stoll(value.GetObjectPointer()->GetText());
					Push(MidoriValue(integer));
				}
				else if (value.IsUnit())
				{
					Push(MidoriValue(static_cast<MidoriValue::MidoriInteger>(0ll)));
				}
				else
				{
					// unreachable
					throw InterpreterException(GenerateRuntimeError("Cannot cast to integer.", GetLine()));
				}

				break;
			}
			case OpCode::CAST_TO_TEXT:
			{
				const MidoriValue& value = Pop();

				if (value.IsBool())
				{
					Push(CollectGarbageThenAllocateObject(value.GetBool() ? MidoriTraceable::MidoriText("true") : MidoriTraceable::MidoriText("false")));
				}
				else if (value.IsInteger())
				{
					Push(CollectGarbageThenAllocateObject(std::to_string(value.GetInteger())));
				}
				else if (value.IsFraction())
				{
					Push(CollectGarbageThenAllocateObject(std::to_string(value.GetFraction())));
				}
				else if (value.IsObjectPointer() && value.GetObjectPointer()->IsText())
				{
					Push(value);
				}
				else if (value.IsUnit())
				{
					Push(CollectGarbageThenAllocateObject(MidoriTraceable::MidoriText("#")));
				}
				else
				{
					// unreachable
					throw InterpreterException(GenerateRuntimeError("Cannot cast to text.", GetLine()));
				}

				break;
			}
			case OpCode::CAST_TO_BOOL:
			{
				const MidoriValue& value = Pop();

				if (value.IsBool())
				{
					Push(value);
				}
				else if (value.IsInteger())
				{
					Push(MidoriValue(value.GetInteger() != 0ll));
				}
				else if (value.IsFraction())
				{
					Push(MidoriValue(value.GetFraction() != 0.0));
				}
				else if (value.IsObjectPointer() && value.GetObjectPointer()->IsText())
				{
					Push(MidoriValue(!value.GetObjectPointer()->GetText().empty()));
				}
				else if (value.IsUnit())
				{
					Push(MidoriValue(false));
				}
				else
				{
					// unreachable
					throw InterpreterException(GenerateRuntimeError("Cannot cast to bool.", GetLine()));
				}

				break;
			}
			case OpCode::CAST_TO_UNIT:
			{
				Pop();
				Push(MidoriValue());
				break;
			}
			case OpCode::LEFT_SHIFT:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(MidoriValue(left.GetInteger() << right.GetInteger()));
				break;
			}
			case OpCode::RIGHT_SHIFT:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(MidoriValue(left.GetInteger() >> right.GetInteger()));
				break;
			}
			case OpCode::BITWISE_AND:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(MidoriValue(left.GetInteger() & right.GetInteger()));
				break;
			}
			case OpCode::BITWISE_OR:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(MidoriValue(left.GetInteger() | right.GetInteger()));
				break;
			}
			case OpCode::BITWISE_XOR:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(MidoriValue(left.GetInteger() ^ right.GetInteger()));
				break;
			}
			case OpCode::BITWISE_NOT:
			{
				const MidoriValue& right = Pop();
				Push(MidoriValue(~right.GetInteger()));
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

				const MidoriValue::MidoriFraction& right_fraction = right.GetFraction();
				const MidoriValue::MidoriFraction& left_fraction = left.GetFraction();

				CheckZeroDivision(right_fraction);

				Push(MidoriValue(left_fraction / right_fraction));
				break;
			}
			case OpCode::MODULO_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();

				const MidoriValue::MidoriFraction& right_fraction = right.GetFraction();
				const MidoriValue::MidoriFraction& left_fraction = left.GetFraction();

				CheckZeroDivision(right_fraction);

				Push(MidoriValue(std::fmod(left_fraction, right_fraction)));
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

				const MidoriValue::MidoriInteger& right_integer = right.GetInteger();
				const MidoriValue::MidoriInteger& left_integer = left.GetInteger();

				CheckZeroDivision(right_integer);

				Push(MidoriValue(left_integer / right_integer));
				break;
			}
			case OpCode::MODULO_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();

				const MidoriValue::MidoriInteger& right_integer = right.GetInteger();
				const MidoriValue::MidoriInteger& left_integer = left.GetInteger();

				CheckZeroDivision(right_integer);

				Push(MidoriValue(left_integer % right_integer));
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
				const MidoriValue& value = Peek();

				int offset = ReadShort();
				if (!value.GetBool())
				{
					m_instruction_pointer += offset;
				}
				break;
			}
			case OpCode::JUMP_IF_TRUE:
			{
				const MidoriValue& value = Peek();

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
			case OpCode::CALL_FOREIGN:
			{
				const MidoriValue& foreign_function_name = Pop();
				int arity = static_cast<int>(ReadByte());
				MidoriTraceable::MidoriText& foreign_function_name_ref = foreign_function_name.GetObjectPointer()->GetText();

				// Platform-specific function loading
#if defined(_WIN32) || defined(_WIN64)
				FARPROC proc = GetProcAddress(m_library_handle, foreign_function_name_ref.c_str());
#else
				void* proc = dlsym(m_library_handle, foreign_function_name_ref.c_str());
#endif
				if (proc == nullptr)
				{
					throw InterpreterException(GenerateRuntimeError(std::format("Failed to load foreign function '{}'.", foreign_function_name_ref), GetLine()));
				}

				std::vector<MidoriValue*> args(arity);
				std::for_each(args.rbegin(), args.rend(), [this](MidoriValue*& value) -> void
					{
						value = std::addressof(Pop());
					});
				MidoriValue return_val;


				void(*ffi)(const std::vector<MidoriValue*>&, MidoriValue*) = reinterpret_cast<void(*)(const std::vector<MidoriValue*>&, MidoriValue*)>(proc);
				ffi(args, &return_val);

				Push(return_val);

				break;
			}
			case OpCode::CALL_DEFINED:
			{
				const MidoriValue& callable = Pop();
				int arity = static_cast<int>(ReadByte());

				MidoriTraceable::Closure& closure = callable.GetObjectPointer()->GetClosure();

				m_closure_stack.emplace_back(std::addressof(closure.m_cell_values));

				if (std::next(m_call_stack_pointer) == m_call_stack_end)
				{
					throw InterpreterException(GenerateRuntimeError("Call stack overflow.", GetLine()));
				}

				// Return address := pop all the arguments and the callee
				*m_call_stack_pointer = { m_current_bytecode , m_base_pointer, m_value_stack_pointer - arity, m_instruction_pointer };
				++m_call_stack_pointer;

				m_current_bytecode = &m_executable_module.m_modules[closure.m_module_index];
				m_instruction_pointer = m_current_bytecode->cbegin();
				m_base_pointer = m_value_stack_pointer - arity;

				break;
			}
			case OpCode::CONSTRUCT_STRUCT:
			{
				int size = static_cast<int>(ReadByte());
				std::vector<MidoriValue> args(size);

				for (int i = size - 1; i >= 0; i -= 1)
				{
					args[i] = Pop();
				}

				std::vector<MidoriValue>& members = Peek().GetObjectPointer()->GetStruct().m_values;
				members = std::move(args);

				break;
			}
			case OpCode::ALLOCATE_STRUCT:
			{
				Push(MidoriValue(AllocateObject(MidoriTraceable::MidoriStruct())));
				break;
			}
			case OpCode::CREATE_CLOSURE:
			{
				int captured_count = static_cast<int>(ReadByte());
				int arity = static_cast<int>(ReadByte());
				int module_index = static_cast<int>(ReadByte());

				Push(MidoriValue(AllocateObject(MidoriTraceable::Closure(std::vector<MidoriTraceable*>(), module_index, arity))));
				if (captured_count == 0)
				{
					break;
				}

				std::vector<MidoriTraceable*>& captured_variables = std::prev(m_value_stack_pointer)->GetObjectPointer()->GetClosure().m_cell_values;

				if (!m_closure_stack.empty())
				{
					const MidoriTraceable::Closure::Environment& parent_closure = *m_closure_stack.back();
					captured_variables = parent_closure;
					captured_count -= static_cast<int>(parent_closure.size());
				}

				for (StackPointer<MidoriValue, s_value_stack_max> it = m_base_pointer; it < m_base_pointer + captured_count; ++it)
				{
					MidoriTraceable* cell_value = AllocateObject(MidoriTraceable::CellValue(*it));
					captured_variables.emplace_back(cell_value);
				}
				break;
			}
			case OpCode::DEFINE_GLOBAL:
			{
				const MidoriValue& value = Pop();
				const std::string& name = ReadGlobalVariable();
				MidoriValue& var = m_global_vars[name];
				var = value;
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
				const std::string& name = ReadGlobalVariable();
				MidoriValue& var = m_global_vars[name];
				var = Peek();
				break;
			}
			case OpCode::GET_LOCAL:
			{
				int offset = static_cast<int>(ReadByte());
				Push(*(m_base_pointer + offset));
				break;
			}
			case OpCode::SET_LOCAL:
			{
				int offset = static_cast<int>(ReadByte());
				MidoriValue& var = *(m_base_pointer + offset);

				const MidoriValue& value = Peek();
				var = value;
				break;
			}
			case OpCode::GET_CELL:
			{
				int offset = static_cast<int>(ReadByte());
				Push((*m_closure_stack.back())[offset]->GetCellValue());
				break;
			}
			case OpCode::SET_CELL:
			{
				int offset = static_cast<int>(ReadByte());
				MidoriValue& var = (*m_closure_stack.back())[offset]->GetCellValue();
				var = Peek();
				break;
			}
			case OpCode::GET_MEMBER:
			{
				int index = static_cast<int>(ReadByte());
				const MidoriValue& value = Pop();
				Push(value.GetObjectPointer()->GetStruct().m_values[index]);
				break;
			}
			case OpCode::SET_MEMBER:
			{
				int index = static_cast<int>(ReadByte());
				const MidoriValue& value = Pop();
				const MidoriValue& var = Peek();
				var.GetObjectPointer()->GetStruct().m_values[index] = value;
				break;
			}
			case OpCode::POP:
			{
				--m_value_stack_pointer;
				break;
			}
			case OpCode::POP_MULTIPLE:
			{
				m_value_stack_pointer -= static_cast<int>(ReadByte());
				break;
			}
			case OpCode::RETURN:
			{
				const MidoriValue& value = Pop();
				m_call_stack_pointer = std::prev(m_call_stack_pointer);

				CallFrame& top_frame = *m_call_stack_pointer;
				m_closure_stack.pop_back();

				m_current_bytecode = top_frame.m_return_module;
				m_base_pointer = top_frame.m_return_bp;
				m_instruction_pointer = top_frame.m_return_ip;
				m_value_stack_pointer = top_frame.m_return_sp;

				Push(value);

				if ((m_call_stack_pointer == m_call_stack_begin) && (m_instruction_pointer == m_current_bytecode->cend()))
				{
					return;
				}

				break;
			}
			case OpCode::HALT:
			{
				DisplayRuntimeError(GenerateRuntimeError("Program halted.", GetLine()));
				break;
			}
			default:
			{
				DisplayRuntimeError(GenerateRuntimeError("Unknown Instruction.", GetLine()));
				break;
			}
			}
		}
	}
	catch (const InterpreterException& e)
	{
		DisplayRuntimeError(e.what());
	}
}
