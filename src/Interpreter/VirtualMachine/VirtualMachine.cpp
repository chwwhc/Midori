#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <execution>

#ifdef DEBUG
#include <iostream>
#endif

void VirtualMachine::DisplayRuntimeError(std::string_view message) noexcept
{
	std::cerr << "\033[31m" << message << "\033[0m" << std::endl;
}

int VirtualMachine::GetLine()
{
	for (int i = 0; i < m_executable.GetProcedureCount(); i += 1)
	{
		if (m_instruction_pointer >= &*m_executable.GetBytecodeStream(i).cbegin() && m_instruction_pointer <= &*std::prev(m_executable.GetBytecodeStream(i).cend()))
		{
			return m_executable.GetLine(static_cast<int>(m_instruction_pointer - &*m_executable.GetBytecodeStream(i).cbegin()), i);
		}
	}

	throw InterpreterException(GenerateRuntimeError("Invalid instruction pointer.", 0));
}

OpCode VirtualMachine::ReadByte() noexcept
{
	OpCode op_code = *m_instruction_pointer;
	++m_instruction_pointer;
	return op_code;
}

int VirtualMachine::ReadShort() noexcept
{
	return static_cast<int>(ReadByte()) | (static_cast<int>(ReadByte()) << 8);
}

int VirtualMachine::ReadThreeBytes() noexcept
{
	return static_cast<int>(ReadByte()) |
		(static_cast<int>(ReadByte()) << 8) |
		(static_cast<int>(ReadByte()) << 16);
}

const MidoriValue& VirtualMachine::ReadConstant(OpCode operand_length) noexcept
{
	int index = 0;

	switch (operand_length)
	{
	case OpCode::CONSTANT:
	{
		index = static_cast<int>(ReadByte());
		break;
	}
	case OpCode::CONSTANT_LONG:
	{
		index = static_cast<int>(ReadByte()) |
			(static_cast<int>(ReadByte()) << 8);
		break;
	}
	case OpCode::CONSTANT_LONG_LONG:
	{
		index = static_cast<int>(ReadByte()) |
			(static_cast<int>(ReadByte()) << 8) |
			(static_cast<int>(ReadByte()) << 16);
		break;
	}
	default:
	{
		break; // unreachable
	}
	}

	return m_executable.GetConstant(index);
}

const std::string& VirtualMachine::ReadGlobalVariable() noexcept
{
	int index = static_cast<int>(ReadByte());
	return m_executable.GetGlobalVariable(index);
}

std::string VirtualMachine::GenerateRuntimeError(std::string_view message, int line) noexcept
{
	MidoriTraceable::CleanUp();
	return MidoriError::GenerateRuntimeError(message, line);
}

void VirtualMachine::PushCallFrame(ValueStackPointer m_return_bp, ValueStackPointer m_return_sp, InstructionPointer m_return_ip)
{
	if (m_call_stack_pointer != m_call_stack_end)
	{
		*m_call_stack_pointer = { m_return_bp, m_return_sp, m_return_ip };
		++m_call_stack_pointer;
		return;
	}

	throw InterpreterException(GenerateRuntimeError("Call stack overflow.", GetLine()));
}

const MidoriValue& VirtualMachine::Peek() const noexcept
{
	const MidoriValue& value = *(std::prev(m_value_stack_pointer));
	return value;
}

MidoriValue& VirtualMachine::Pop() noexcept
{
	MidoriValue& value = *(--m_value_stack_pointer);
	return value;
}

void VirtualMachine::PromoteCells() noexcept
{
	std::for_each
	(
		std::execution::par,  
		m_cells_to_promote.begin(),
		m_cells_to_promote.end(),
		[](MidoriTraceable::CellValue* cell) -> void 
		{
			cell->m_heap_value = *cell->m_stack_value_ref;
			cell->m_is_on_heap = true;
		}
	);
	m_cells_to_promote.clear();
}

void VirtualMachine::CheckIndexBounds(const MidoriValue& index, MidoriValue::MidoriInteger size)
{
	MidoriValue::MidoriInteger val = index.GetInteger();
	if (val < 0ll || val >= size)
	{
		throw InterpreterException(GenerateRuntimeError(std::format("Index out of bounds at index: {}.", val), GetLine()));
	}
}

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetGlobalTableGarbageCollectionRoots()
{
	MidoriTraceable::GarbageCollectionRoots roots;

	std::for_each
	(
		std::execution::seq,
		m_global_vars.cbegin(), 
		m_global_vars.cend(), 
		[&roots](const GlobalVariables::value_type& pair) -> void
		{
			if (pair.second.IsPointer())
			{
				roots.emplace(static_cast<MidoriTraceable*>(pair.second.GetPointer()));
			}
		}
	);

	return roots;
}

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetValueStackGarbageCollectionRoots()
{
	MidoriTraceable::GarbageCollectionRoots roots;

	std::for_each_n
	(
		std::execution::seq,
		m_value_stack->begin(), 
		m_value_stack_pointer - m_value_stack->begin(), 
		[&roots](MidoriValue& value) -> void
		{
			if (value.IsPointer())
			{
				roots.emplace(static_cast<MidoriTraceable*>(value.GetPointer()));
			}
		}
	);

	return roots;
}

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetGarbageCollectionRoots()
{
	MidoriTraceable::GarbageCollectionRoots stack_roots = GetValueStackGarbageCollectionRoots();
	MidoriTraceable::GarbageCollectionRoots global_roots = GetGlobalTableGarbageCollectionRoots();

	stack_roots.insert(global_roots.cbegin(), global_roots.cend());
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
	std::cout << "\033[34m" << "\nBefore garbage collection:"; // Blue
	MidoriTraceable::PrintMemoryTelemetry();
#endif
	m_garbage_collector.ReclaimMemory(std::move(roots));
#ifdef DEBUG
	std::cout << "\nAfter garbage collection:";
	MidoriTraceable::PrintMemoryTelemetry();
	std::cout << "\033[0m"; // Reset color
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
			std::for_each
			(
				std::execution::seq,
				m_base_pointer, 
				m_value_stack_pointer, 
				[](MidoriValue& value) -> void
				{
					std::cout << "[ " << value.ToString() << " ]";
				}
			);
			std::cout << std::endl;
			int dbg_instruction_pointer = -1;
			int dbg_proc_index = -1;
			for (int i = 0; i < m_executable.GetProcedureCount(); i += 1)
			{
				if (&*m_instruction_pointer >= &*m_executable.GetBytecodeStream(i).cbegin() && &*m_instruction_pointer <= &*std::prev(m_executable.GetBytecodeStream(i).cend()))
				{
					dbg_proc_index = i;
					dbg_instruction_pointer = static_cast<int>(m_instruction_pointer - &*m_executable.GetBytecodeStream(i).cbegin());
				}
			}
			Disassembler::DisassembleInstruction(m_executable, dbg_proc_index, dbg_instruction_pointer);
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
				Push();
				break;
			}
			case OpCode::OP_TRUE:
			{
				Push(true);
				break;
			}
			case OpCode::OP_FALSE:
			{
				Push(false);
				break;
			}
			case OpCode::CREATE_ARRAY:
			{
				int count = ReadThreeBytes();
				std::vector<MidoriValue> arr(count);

				std::for_each
				(
					std::execution::seq,
					arr.rbegin(),
					arr.rend(), 
					[this](MidoriValue& value) -> void
					{
						value = Pop();
					}
				);
				Push(MidoriTraceable::AllocateTraceable(std::move(arr)));
				break;
			}
			case OpCode::GET_ARRAY:
			{
				int num_indices = static_cast<int>(ReadByte());
				std::vector<MidoriValue> indices(num_indices);

				std::for_each
				(
					std::execution::seq,
					indices.rbegin(), 
					indices.rend(), 
					[this](MidoriValue& value) -> void
					{
						value = Pop();
					}
				);

				MidoriValue& arr = Pop();
				std::vector<MidoriValue>& arr_ref = arr.GetPointer()->GetArray();
				MidoriValue::MidoriInteger arr_size = static_cast<MidoriValue::MidoriInteger>(arr_ref.size());

				for (MidoriValue& index : indices)
				{
					CheckIndexBounds(index, arr_size);

					MidoriValue& next_val = arr_ref[static_cast<size_t>(index.GetInteger())];

					if (&index != &indices.back())
					{
						arr_ref = next_val.GetPointer()->GetArray();
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

				std::for_each
				(
					std::execution::seq,
					indices.rbegin(), 
					indices.rend(), 
					[this](MidoriValue& value) -> void
					{
						value = Pop();
					}
				);

				MidoriValue& arr = Pop();
				std::vector<MidoriValue>& arr_ref = arr.GetPointer()->GetArray();
				MidoriValue::MidoriInteger arr_size = static_cast<MidoriValue::MidoriInteger>(arr_ref.size());

				for (MidoriValue& index : indices)
				{
					CheckIndexBounds(index, arr_size);

					size_t i = static_cast<size_t>(index.GetInteger());

					if (&index != &indices.back())
					{
						arr_ref = arr_ref[i].GetPointer()->GetArray();
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

				if (value.IsInteger())
				{
					Push(static_cast<MidoriValue::MidoriFraction>(value.GetInteger()));
				}
				else if (value.IsFraction())
				{
					Push(value);
				}
				else if (value.IsPointer() && value.GetPointer()->IsText())
				{
					MidoriValue::MidoriFraction fraction = std::stod(value.GetPointer()->GetText());
					Push(fraction);
				}
				else
				{
					throw InterpreterException(GenerateRuntimeError("Unable to cast to Fraction.", GetLine()));
				}

				break;
			}
			case OpCode::CAST_TO_INTEGER:
			{
				const MidoriValue& value = Pop();

				if (value.IsInteger())
				{
					Push(value);
				}
				else if (value.IsFraction())
				{
					Push(static_cast<MidoriValue::MidoriInteger>(value.GetFraction()));
				}
				else if (value.IsPointer() && value.GetPointer()->IsText())
				{
					MidoriValue::MidoriInteger integer = std::stoll(value.GetPointer()->GetText());
					Push(integer);
				}
				else
				{
					throw InterpreterException(GenerateRuntimeError("Unable to cast to Integer.", GetLine()));
				}

				break;
			}
			case OpCode::CAST_TO_TEXT:
			{
				const MidoriValue& value = Pop();

				if (value.IsBool())
				{
					Push(CollectGarbageThenAllocateTraceable(value.GetBool() ? MidoriTraceable::MidoriText("true") : MidoriTraceable::MidoriText("false")));
				}
				else if (value.IsInteger())
				{
					Push(CollectGarbageThenAllocateTraceable(std::to_string(value.GetInteger())));
				}
				else if (value.IsFraction())
				{
					Push(CollectGarbageThenAllocateTraceable(std::to_string(value.GetFraction())));
				}
				else if (value.IsPointer())
				{
					const MidoriTraceable& ptr = *value.GetPointer();
					if (ptr.IsText())
					{
						Push(value);
					}
					else
					{
						Push(CollectGarbageThenAllocateTraceable(ptr.ToString()));
					}
				}
				else if (value.IsUnit())
				{
					Push(CollectGarbageThenAllocateTraceable(MidoriTraceable::MidoriText("#")));
				}
				else
				{
					throw InterpreterException(GenerateRuntimeError("Unable to cast to Text.", GetLine()));
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
					Push(value.GetInteger() != 0ll);
				}
				else if (value.IsFraction())
				{
					Push(value.GetFraction() != 0.0);
				}
				else
				{
					throw InterpreterException(GenerateRuntimeError("Unable to cast to Bool.", GetLine()));
				}

				break;
			}
			case OpCode::CAST_TO_UNIT:
			{
				Pop();
				Push();
				break;
			}
			case OpCode::LEFT_SHIFT:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() << right.GetInteger());
				break;
			}
			case OpCode::RIGHT_SHIFT:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() >> right.GetInteger());
				break;
			}
			case OpCode::BITWISE_AND:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() & right.GetInteger());
				break;
			}
			case OpCode::BITWISE_OR:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() | right.GetInteger());
				break;
			}
			case OpCode::BITWISE_XOR:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() ^ right.GetInteger());
				break;
			}
			case OpCode::BITWISE_NOT:
			{
				const MidoriValue& right = Pop();
				Push(~right.GetInteger());
				break;
			}
			case OpCode::ADD_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() + right.GetFraction());
				break;
			}
			case OpCode::SUBTRACT_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() - right.GetFraction());
				break;
			}
			case OpCode::MULTIPLY_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() * right.GetFraction());
				break;
			}
			case OpCode::DIVIDE_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();

				const MidoriValue::MidoriFraction& right_fraction = right.GetFraction();
				const MidoriValue::MidoriFraction& left_fraction = left.GetFraction();

				CheckZeroDivision(right_fraction);

				Push(left_fraction / right_fraction);
				break;
			}
			case OpCode::MODULO_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();

				const MidoriValue::MidoriFraction& right_fraction = right.GetFraction();
				const MidoriValue::MidoriFraction& left_fraction = left.GetFraction();

				CheckZeroDivision(right_fraction);

				Push(std::fmod(left_fraction, right_fraction));
				break;
			}
			case OpCode::ADD_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() + right.GetInteger());
				break;
			}
			case OpCode::SUBTRACT_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() - right.GetInteger());
				break;
			}
			case OpCode::MULTIPLY_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() * right.GetInteger());
				break;
			}
			case OpCode::DIVIDE_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();

				const MidoriValue::MidoriInteger& right_integer = right.GetInteger();
				const MidoriValue::MidoriInteger& left_integer = left.GetInteger();

				CheckZeroDivision(right_integer);

				Push(left_integer / right_integer);
				break;
			}
			case OpCode::MODULO_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();

				const MidoriValue::MidoriInteger& right_integer = right.GetInteger();
				const MidoriValue::MidoriInteger& left_integer = left.GetInteger();

				CheckZeroDivision(right_integer);

				Push(left_integer % right_integer);
				break;
			}
			case OpCode::CONCAT_ARRAY:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();

				const MidoriTraceable::MidoriArray& left_value_vector_ref = left.GetPointer()->GetArray();
				const MidoriTraceable::MidoriArray& right_value_vector_ref = right.GetPointer()->GetArray();

				MidoriTraceable::MidoriArray new_value_vector(left_value_vector_ref);
				new_value_vector.insert(new_value_vector.end(),
					right_value_vector_ref.begin(),
					right_value_vector_ref.end());

				Push(MidoriTraceable::AllocateTraceable(std::move(new_value_vector)));
				break;
			}
			case OpCode::CONCAT_TEXT:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();

				const MidoriTraceable::MidoriText& left_value_string_ref = left.GetPointer()->GetText();
				const MidoriTraceable::MidoriText& right_value_string_ref = right.GetPointer()->GetText();

				MidoriTraceable::MidoriText new_value_string = left_value_string_ref + right_value_string_ref;

				Push(CollectGarbageThenAllocateTraceable(std::move(new_value_string)));
				break;
			}
			case OpCode::EQUAL_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() == right.GetFraction());
				break;
			}
			case OpCode::NOT_EQUAL_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() != right.GetFraction());
				break;
			}
			case OpCode::GREATER_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() > right.GetFraction());
				break;
			}
			case OpCode::GREATER_EQUAL_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() >= right.GetFraction());
				break;
			}
			case OpCode::LESS_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() < right.GetFraction());
				break;
			}
			case OpCode::LESS_EQUAL_FRACTION:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetFraction() <= right.GetFraction());
				break;
			}
			case OpCode::EQUAL_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() == right.GetInteger());
				break;
			}
			case OpCode::NOT_EQUAL_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() != right.GetInteger());
				break;
			}
			case OpCode::GREATER_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() > right.GetInteger());
				break;
			}
			case OpCode::GREATER_EQUAL_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() >= right.GetInteger());
				break;
			}
			case OpCode::LESS_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() < right.GetInteger());
				break;
			}
			case OpCode::LESS_EQUAL_INTEGER:
			{
				const MidoriValue& right = Pop();
				const MidoriValue& left = Pop();
				Push(left.GetInteger() <= right.GetInteger());
				break;
			}
			case OpCode::NOT:
			{
				const MidoriValue& value = Pop();
				Push(!value.GetBool());
				break;
			}
			case OpCode::NEGATE_FRACTION:
			{
				const MidoriValue& value = Pop();
				Push(-value.GetFraction());
				break;
			}
			case OpCode::NEGATE_INTEGER:
			{
				const MidoriValue& value = Pop();
				Push(-value.GetInteger());
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
				MidoriTraceable::MidoriText& foreign_function_name_ref = foreign_function_name.GetPointer()->GetText();

				// Platform-specific function loading
#ifdef _WIN32
				FARPROC proc = GetProcAddress(m_library_handle, foreign_function_name_ref.c_str());
#else
				void* proc = dlsym(m_library_handle, foreign_function_name_ref.c_str());
#endif
				if (proc == nullptr)
				{
					throw InterpreterException(GenerateRuntimeError(std::format("Failed to load foreign function '{}'.", foreign_function_name_ref), GetLine()));
				}

				std::vector<MidoriValue*> args(arity);
				std::for_each
				(
					std::execution::seq,
					args.rbegin(), 
					args.rend(),
					[this](MidoriValue*& value) -> void
					{
						value = std::addressof(Pop());
					}
				);
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

				MidoriTraceable::Closure& closure = callable.GetPointer()->GetClosure();

				m_closure_stack.emplace_back(std::addressof(closure.m_cell_values));

				// Return address := pop all the arguments and the callee
				PushCallFrame(m_base_pointer, m_value_stack_pointer - arity, m_instruction_pointer);

				m_instruction_pointer = m_executable.GetBytecodeStream(closure.m_proc_index)[0];
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

				std::vector<MidoriValue>& members = Peek().GetPointer()->GetStruct().m_values;
				members = std::move(args);

				break;
			}
			case OpCode::ALLOCATE_STRUCT:
			{
				Push(MidoriTraceable::AllocateTraceable(MidoriTraceable::MidoriStruct()));
				break;
			}
			case OpCode::CREATE_CLOSURE:
			{
				int captured_count = static_cast<int>(ReadByte());
				int arity = static_cast<int>(ReadByte());
				int proc_index = static_cast<int>(ReadByte());

				Push(MidoriTraceable::AllocateTraceable(MidoriTraceable::Closure(MidoriTraceable::Closure::Environment(), proc_index, arity)));
				if (captured_count == 0)
				{
					break;
				}

				MidoriTraceable::Closure::Environment& captured_variables = std::prev(m_value_stack_pointer)->GetPointer()->GetClosure().m_cell_values;

				if (!m_closure_stack.empty())
				{
					const MidoriTraceable::Closure::Environment& parent_closure = *m_closure_stack.back();
					captured_variables = parent_closure;
					captured_count -= static_cast<int>(parent_closure.size());
				}

				for (ValueStackPointer it = m_base_pointer; it < m_base_pointer + captured_count; ++it)
				{
					MidoriValue* stack_value_ref = &*it;
					MidoriValue cell_value = CollectGarbageThenAllocateTraceable(stack_value_ref);
					captured_variables.emplace_back(cell_value);
					m_cells_to_promote.emplace_back(std::addressof(cell_value.GetPointer()->GetCellValue()));
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
				Push(m_global_vars[name]);
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
				const MidoriValue& cell_value = (*m_closure_stack.back())[offset].GetPointer()->GetCellValue().GetValue();
				Push(cell_value);
				break;
			}
			case OpCode::SET_CELL:
			{
				int offset = static_cast<int>(ReadByte());
				MidoriValue& cell_value = (*m_closure_stack.back())[offset].GetPointer()->GetCellValue().GetValue();
				cell_value = Peek();
				break;
			}
			case OpCode::GET_MEMBER:
			{
				int index = static_cast<int>(ReadByte());
				const MidoriValue& value = Pop();
				Push(value.GetPointer()->GetStruct().m_values[index]);
				break;
			}
			case OpCode::SET_MEMBER:
			{
				int index = static_cast<int>(ReadByte());
				const MidoriValue& value = Pop();
				const MidoriValue& var = Peek();
				var.GetPointer()->GetStruct().m_values[index] = value;
				break;
			}
			case OpCode::POP:
			{
				--m_value_stack_pointer;
				break;
			}
			case OpCode::POP_SCOPE:
			{
				// on scope exit, promote all cells to heap
				PromoteCells();

				m_value_stack_pointer -= static_cast<int>(ReadByte());
				break;
			}
			case OpCode::RETURN:
			{
				// on return, promote all cells to heap
				PromoteCells();

				const MidoriValue& value = Pop();
				m_call_stack_pointer = std::prev(m_call_stack_pointer);

				CallFrame& top_frame = *m_call_stack_pointer;
				m_closure_stack.pop_back();

				m_base_pointer = top_frame.m_return_bp;
				m_instruction_pointer = top_frame.m_return_ip;
				m_value_stack_pointer = top_frame.m_return_sp;

				Push(value);

				break;
			}
			case OpCode::HALT:
			{
				return;
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
