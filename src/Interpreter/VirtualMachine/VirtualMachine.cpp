#include "VirtualMachine.h"
#include "Utility/Disassembler/Disassembler.h"
#include "Common/Printer/Printer.h"
#include "Common/Constant/Constant.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <execution>
#include <format>

VirtualMachine::VirtualMachine(MidoriExecutable&& executable) noexcept : m_executable(std::move(executable)), m_garbage_collector(m_executable.GetConstantRoots())
{
	constexpr int runtime_startup_proc_index = 0;
	MidoriTraceable* sentinel_closure = MidoriTraceable::AllocateTraceable(MidoriClosure());

	m_global_vars.resize(static_cast<size_t>(m_executable.GetGlobalVariableCount()));
	m_instruction_pointer = &*m_executable.GetBytecodeStream(runtime_startup_proc_index).cbegin();
	(*m_call_stack)[0u] = CallFrame{ nullptr, nullptr, nullptr, sentinel_closure };
}

VirtualMachine::~VirtualMachine()
{
	m_garbage_collector.CleanUp();
#ifdef _WIN32
	FreeLibrary(m_library_handle);
#else
	dlclose(m_library_handle);
#endif
}

void VirtualMachine::TerminateExecution(std::string_view message) noexcept
{
	Printer::Print<Printer::Color::RED>(message);

	std::destroy_at(this);

	std::exit(EXIT_FAILURE);
}

int VirtualMachine::GetLine() noexcept
{
	for (int i : std::views::iota(0, m_executable.GetProcedureCount()))
	{
		if (m_instruction_pointer >= &*m_executable.GetBytecodeStream(i).cbegin() && m_instruction_pointer <= &*std::prev(m_executable.GetBytecodeStream(i).cend()))
		{
			return m_executable.GetLine(static_cast<int>(m_instruction_pointer - &*m_executable.GetBytecodeStream(i).cbegin()), i);
		}
	}

	TerminateExecution(GenerateRuntimeError("Invalid instruction pointer.", 0));
	return -1; // unreachable
}

OpCode VirtualMachine::ReadByte() noexcept
{
	OpCode op_code = *m_instruction_pointer;
	++m_instruction_pointer;
	return op_code;
}

#if defined(MIDORI_LITTLE_ENDIAN)
int VirtualMachine::ReadShort() noexcept
{
	int value = static_cast<int>(*reinterpret_cast<const uint16_t*>(m_instruction_pointer));
	m_instruction_pointer += 2;
	return value;
}

int VirtualMachine::ReadThreeBytes() noexcept
{
	int value = static_cast<int>(*reinterpret_cast<const uint16_t*>(m_instruction_pointer)) |
		(static_cast<int>(m_instruction_pointer[2]) << 16);
	m_instruction_pointer += 3;
	return value;
}
#elif defined(MIDORI_BIG_ENDIAN)
int VirtualMachine::ReadShort() noexcept
{
	int value = (static_cast<int>(m_instruction_pointer[0]) << 8) |
		static_cast<int>(*reinterpret_cast<const uint8_t*>(m_instruction_pointer + 1));
	m_instruction_pointer += 2;
	return value;
}

int VirtualMachine::ReadThreeBytes() noexcept
{
	int value = (static_cast<int>(m_instruction_pointer[0]) << 16) |
		(static_cast<int>(m_instruction_pointer[1]) << 8) |
		static_cast<int>(*reinterpret_cast<const uint8_t*>(m_instruction_pointer + 2));
	m_instruction_pointer += 3;
	return value;
}
#else
#error "Endianness not defined!"
#endif

MidoriInteger VirtualMachine::ReadIntegerConstant() noexcept
{
	MidoriInteger value = *reinterpret_cast<const MidoriInteger*>(m_instruction_pointer);
	m_instruction_pointer += sizeof(MidoriInteger);
	return value;
}

MidoriFraction VirtualMachine::ReadFractionConstant() noexcept
{
	MidoriFraction value = *reinterpret_cast<const MidoriFraction*>(m_instruction_pointer);
	m_instruction_pointer += sizeof(MidoriFraction);
	return value;
}

const MidoriValue& VirtualMachine::ReadConstant(OpCode operand_length) noexcept
{
	int index = 0;

	switch (operand_length)
	{
	case OpCode::LOAD_CONSTANT:
	{
		index = static_cast<int>(ReadByte());
		break;
	}
	case OpCode::LOAD_CONSTANT_LONG:
	{
		index = static_cast<int>(ReadByte()) |
			(static_cast<int>(ReadByte()) << 8);
		break;
	}
	case OpCode::LOAD_CONSTANT_LONG_LONG:
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

int VirtualMachine::ReadGlobalVariable() noexcept
{
	int index = static_cast<int>(ReadByte());
	return index;
}

std::string VirtualMachine::GenerateRuntimeError(std::string_view message, int line) noexcept
{
	m_garbage_collector.CleanUp();
	return MidoriError::GenerateRuntimeError(message, line);
}

void VirtualMachine::PushCallFrame(ValueStackPointer return_bp, ValueStackPointer return_sp, InstructionPointer return_ip, MidoriTraceable* closure_ptr) noexcept
{
	if (m_call_stack_pointer <= m_call_stack_end) [[likely]]
		{
			*m_call_stack_pointer = { return_bp, return_sp, return_ip, closure_ptr };
			++m_call_stack_pointer;
			return;
		}
	else [[unlikely]]
		{
			TerminateExecution(GenerateRuntimeError("Call stack overflow.", GetLine()));
		}
}

MidoriValue& VirtualMachine::Peek() noexcept
{
	return *std::prev(m_value_stack_pointer);
}

MidoriValue& VirtualMachine::Pop() noexcept
{
	MidoriValue& value = *(--m_value_stack_pointer);
	return value;
}

void VirtualMachine::PromoteCells() noexcept
{
	for (size_t i = 0u; i < m_cell_promotion_count; i += 1u)
	{
		MidoriCellValue* cell = m_cells_to_promote[i];
		if (cell->m_stack_value_ref >= m_value_stack_base_pointer)
		{
			cell->m_heap_value = *cell->m_stack_value_ref;
			cell->m_is_on_heap = true;
		}
	}

	m_cell_promotion_count = 0u;
}

void VirtualMachine::CheckIndexBounds(const MidoriValue& index, MidoriInteger size) noexcept
{
	MidoriInteger val = index.GetInteger();
	if (val < 0ll || val >= size)
	{
		TerminateExecution(GenerateRuntimeError(std::format("Index out of bounds at index: {}.", val), GetLine()));
	}
}

void VirtualMachine::CheckNewArraySize(MidoriInteger size) noexcept
{
	if (size < 0)
	{
		TerminateExecution(GenerateRuntimeError("Array size cannot be negative.", GetLine()));
	}
	else if (size > MAX_ARRAY_SIZE)
	{
		TerminateExecution(GenerateRuntimeError("Array size exceeds maximum array size.", GetLine()));
	}
}

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetGlobalTableGarbageCollectionRoots() const noexcept
{
	MidoriTraceable::GarbageCollectionRoots roots;
	roots.reserve(m_global_vars.size());

	std::ranges::for_each
	(
		m_global_vars,
		[&roots](const MidoriValue& val) -> void
		{
			if (val.IsPointer())
			{
				roots.emplace(val.GetPointer());
			}
		}
	);

	return roots;
}

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetValueStackGarbageCollectionRoots() const noexcept
{
	MidoriTraceable::GarbageCollectionRoots roots;
	roots.reserve((m_value_stack_pointer - m_value_stack_begin) + (m_call_stack_pointer - m_call_stack_begin));

	std::for_each_n
	(
		std::execution::seq,
		m_value_stack->begin(),
		m_value_stack_pointer - m_value_stack_begin,
		[&roots](const MidoriValue& value) -> void
		{
			if (value.IsPointer())
			{
				roots.emplace(value.GetPointer());
			}
		}
	);

	std::for_each_n
	(
		std::execution::seq,
		m_call_stack_begin,
		m_call_stack_pointer - m_call_stack_begin,
		[&roots](CallFrame& call_frame) -> void
		{
			roots.emplace(call_frame.m_closure);
		}
	);
	roots.emplace((*m_call_stack)[0u].m_closure); // Sentinel closure

	return roots;
}

MidoriTraceable::GarbageCollectionRoots VirtualMachine::GetGarbageCollectionRoots() const noexcept
{
	MidoriTraceable::GarbageCollectionRoots stack_roots = GetValueStackGarbageCollectionRoots();
	MidoriTraceable::GarbageCollectionRoots global_roots = GetGlobalTableGarbageCollectionRoots();

	stack_roots.insert(global_roots.cbegin(), global_roots.cend());
	return stack_roots;
}

void VirtualMachine::CollectGarbage() noexcept
{
	if (MidoriTraceable::s_total_bytes_allocated - MidoriTraceable::s_static_bytes_allocated < s_garbage_collection_threshold)
	{
		return;
	}

	MidoriTraceable::GarbageCollectionRoots roots = GetGarbageCollectionRoots();
	if (roots.empty()) [[unlikely]]
		{
			return;
		}
	else [[likely]]
		{
#ifdef DEBUG
			Printer::Print<Printer::Color::BLUE>("\nBefore garbage collection:");
			m_garbage_collector.PrintMemoryTelemetry();
#endif
			m_garbage_collector.ReclaimMemory(std::move(roots));
#ifdef DEBUG
			Printer::Print<Printer::Color::BLUE>("\nAfter garbage collection:");
			m_garbage_collector.PrintMemoryTelemetry();
#endif
		}
}

void VirtualMachine::Execute() noexcept
{
#ifdef _WIN32
	m_library_handle = LoadLibrary("./MidoriStdLib.dll");
#else
	m_library_handle = dlopen("./libMidoriStdLib.so", RTLD_LAZY);
#endif

	if (m_library_handle == NULL) [[unlikely]]
		{
#ifdef _WIN32
			FreeLibrary(m_library_handle);
#else
			dlclose(m_library_handle);
#endif
			TerminateExecution("Failed to load the standard library.");
		}

			while (true)
			{
#ifdef DEBUG
				Printer::Print("          ");
				std::for_each
				(
					std::execution::seq,
					m_value_stack_base_pointer,
					m_value_stack_pointer,
					[](MidoriValue& value) -> void
					{
						Printer::Print(("[ "s + value.ToText().GetCString() + " ]"s));
					}
				);
				Printer::Print("\n");
				int dbg_instruction_pointer = -1;
				int dbg_proc_index = -1;

				for (int i : std::views::iota(0, m_executable.GetProcedureCount()))
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
				case OpCode::LOAD_CONSTANT:
				case OpCode::LOAD_CONSTANT_LONG:
				case OpCode::LOAD_CONSTANT_LONG_LONG:
				{
					Push(ReadConstant(instruction));
					break;
				}
				case OpCode::INTEGER_CONSTANT:
				{
					Push(ReadIntegerConstant());
					break;
				}
				case OpCode::FRACTION_CONSTANT:
				{
					Push(ReadFractionConstant());
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
					MidoriArray arr(count);

					for (int i = count - 1; i >= 0; i -= 1)
					{
						arr[i] = Pop();
					}

					Push(MidoriTraceable::AllocateTraceable(std::move(arr)));
					CollectGarbage();
					break;
				}
				case OpCode::GET_ARRAY:
				{
					int num_indices = static_cast<int>(ReadByte());
					MidoriArray indices(num_indices);

					for (int i = num_indices - 1; i >= 0; i -= 1)
					{
						indices[i] = Pop();
					}

					MidoriValue& arr = Pop();
					MidoriArray& arr_ref = arr.GetPointer()->GetArray();
					MidoriInteger arr_size = static_cast<MidoriInteger>(arr_ref.GetLength());

					for (int i = 0; i < num_indices; i += 1)
					{
						MidoriValue& index = indices[i];
						CheckIndexBounds(index, arr_size);

						MidoriValue& next_val = arr_ref[static_cast<int>(index.GetInteger())];

						if (i != num_indices - 1)
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
					MidoriArray indices(num_indices);

					for (int i = num_indices - 1; i >= 0; i -= 1)
					{
						indices[i] = Pop();
					}

					MidoriValue& arr = Pop();
					MidoriArray& arr_ref = arr.GetPointer()->GetArray();
					MidoriInteger arr_size = static_cast<MidoriInteger>(arr_ref.GetLength());

					for (int i = 0; i < num_indices; i += 1)
					{
						MidoriValue& index = indices[i];
						CheckIndexBounds(index, arr_size);
						MidoriValue& next_val = arr_ref[static_cast<int>(index.GetInteger())];
						if (i != num_indices - 1)
						{
							arr_ref = next_val.GetPointer()->GetArray();
						}
						else
						{
							next_val = value_to_set;
						}
					}

					Push(value_to_set);
					break;
				}
				case OpCode::DUP_ARRAY:
				{
					MidoriValue& size = Pop();
					MidoriValue& arr = Pop();
					MidoriArray& arr_ref = arr.GetPointer()->GetArray();

					MidoriInteger new_size = size.GetInteger() * arr_ref.GetLength();
					CheckNewArraySize(new_size);

					MidoriArray new_arr(static_cast<int>(new_size));
					for (int i = 0; i < static_cast<int>(new_size); i += 1)
					{
						new_arr[i] = arr_ref[i % arr_ref.GetLength()];
					}

					Push(MidoriTraceable::AllocateTraceable(std::move(new_arr)));
					CollectGarbage();
					break;
				}
				case OpCode::CAST_TO_FRACTION:
				{
					const MidoriValue& value = Pop();

					if (value.IsInteger())
					{
						Push(static_cast<MidoriFraction>(value.GetInteger()));
					}
					else if (value.IsFraction())
					{
						Push(value);
					}
					else if (value.IsPointer() && value.GetPointer()->IsText())
					{
						MidoriFraction fraction = value.GetPointer()->GetText().ToFraction();
						Push(fraction);
					}
					else
					{
						TerminateExecution(GenerateRuntimeError("Unable to cast to Fraction.", GetLine()));
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
						Push(static_cast<MidoriInteger>(value.GetFraction()));
					}
					else if (value.IsPointer() && value.GetPointer()->IsText())
					{
						MidoriInteger integer = value.GetPointer()->GetText().ToInteger();
						Push(integer);
					}
					else
					{
						TerminateExecution(GenerateRuntimeError("Unable to cast to Integer.", GetLine()));
					}

					break;
				}
				case OpCode::CAST_TO_TEXT:
				{
					const MidoriValue& value = Pop();

					if (value.IsBool())
					{
						Push(MidoriTraceable::AllocateTraceable(value.GetBool() ? MidoriText("true") : MidoriText("false")));
					}
					else if (value.IsInteger())
					{
						Push(MidoriTraceable::AllocateTraceable(MidoriText::FromInteger(value.GetInteger())));
					}
					else if (value.IsFraction())
					{
						Push(MidoriTraceable::AllocateTraceable(MidoriText::FromFraction(value.GetFraction())));
					}
					else if (value.IsPointer())
					{
						MidoriTraceable& ptr = *value.GetPointer();
						if (ptr.IsText())
						{
							Push(value);
						}
						else
						{
							Push(MidoriTraceable::AllocateTraceable(ptr.ToText()));
						}
					}
					else if (value.IsUnit())
					{
						Push(MidoriTraceable::AllocateTraceable(MidoriText("()")));
					}
					else
					{
						TerminateExecution(GenerateRuntimeError("Unable to cast to Text.", GetLine()));
					}
					CollectGarbage();
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
						TerminateExecution(GenerateRuntimeError("Unable to cast to Bool.", GetLine()));
					}

					break;
				}
				case OpCode::CAST_TO_UNIT:
				{
					Peek() = {};
					break;
				}
				case OpCode::LEFT_SHIFT:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() << right.GetInteger();
					break;
				}
				case OpCode::RIGHT_SHIFT:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() >> right.GetInteger();

					break;
				}
				case OpCode::BITWISE_AND:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() & right.GetInteger();

					break;
				}
				case OpCode::BITWISE_OR:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() | right.GetInteger();

					break;
				}
				case OpCode::BITWISE_XOR:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() ^ right.GetInteger();

					break;
				}
				case OpCode::BITWISE_NOT:
				{
					MidoriValue& right = Peek();

					right = ~right.GetInteger();

					break;
				}
				case OpCode::ADD_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() + right.GetFraction();

					break;
				}
				case OpCode::SUBTRACT_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() - right.GetFraction();

					break;
				}
				case OpCode::MULTIPLY_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() * right.GetFraction();

					break;
				}
				case OpCode::DIVIDE_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() / right.GetFraction();

					break;
				}
				case OpCode::MODULO_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = std::fmod(left.GetFraction(), right.GetFraction());

					break;
				}
				case OpCode::ADD_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() + right.GetInteger();

					break;
				}
				case OpCode::SUBTRACT_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() - right.GetInteger();

					break;
				}
				case OpCode::MULTIPLY_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() * right.GetInteger();

					break;
				}
				case OpCode::DIVIDE_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() / right.GetInteger();

					break;
				}
				case OpCode::MODULO_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() % right.GetInteger();

					break;
				}
				case OpCode::CONCAT_ARRAY:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					MidoriArray& left_value_vector_ref = left.GetPointer()->GetArray();
					MidoriArray& right_value_vector_ref = right.GetPointer()->GetArray();
					MidoriArray result = MidoriArray::Concatenate(left_value_vector_ref, right_value_vector_ref);

					left = MidoriTraceable::AllocateTraceable(std::move(result));
					CollectGarbage();
					break;
				}
				case OpCode::CONCAT_TEXT:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					MidoriText& left_value_string_ref = left.GetPointer()->GetText();
					MidoriText& right_value_string_ref = right.GetPointer()->GetText();

					MidoriText result = MidoriText::Concatenate(left_value_string_ref, right_value_string_ref);

					left = MidoriTraceable::AllocateTraceable(std::move(result));
					CollectGarbage();
					break;
				}
				case OpCode::EQUAL_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() == right.GetFraction();

					break;
				}
				case OpCode::NOT_EQUAL_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() != right.GetFraction();

					break;
				}
				case OpCode::GREATER_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() > right.GetFraction();

					break;
				}
				case OpCode::GREATER_EQUAL_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() >= right.GetFraction();

					break;
				}
				case OpCode::LESS_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() < right.GetFraction();

					break;
				}
				case OpCode::LESS_EQUAL_FRACTION:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetFraction() <= right.GetFraction();

					break;
				}
				case OpCode::EQUAL_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() == right.GetInteger();

					break;
				}
				case OpCode::NOT_EQUAL_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() != right.GetInteger();

					break;
				}
				case OpCode::GREATER_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() > right.GetInteger();

					break;
				}
				case OpCode::GREATER_EQUAL_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() >= right.GetInteger();

					break;
				}
				case OpCode::LESS_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() < right.GetInteger();

					break;
				}
				case OpCode::LESS_EQUAL_INTEGER:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetInteger() <= right.GetInteger();

					break;
				}
				case OpCode::EQUAL_TEXT:
				{
					const MidoriValue& right = Pop();
					MidoriValue& left = Peek();

					left = left.GetPointer()->GetText() == right.GetPointer()->GetText();

					break;
				}
				case OpCode::NOT:
				{
					MidoriValue& value = Peek();
					value = !value.GetBool();
					break;
				}
				case OpCode::NEGATE_FRACTION:
				{
					MidoriValue& value = Peek();
					value = -value.GetFraction();
					break;
				}
				case OpCode::NEGATE_INTEGER:
				{
					MidoriValue& value = Peek();
					value = -value.GetInteger();
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
				case OpCode::IF_INTEGER_LESS:
				{
					int offset = ReadShort();
					MidoriInteger right = Pop().GetInteger();
					MidoriInteger left = Pop().GetInteger();

					if (!(left < right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_INTEGER_LESS_EQUAL:
				{
					int offset = ReadShort();
					MidoriInteger right = Pop().GetInteger();
					MidoriInteger left = Pop().GetInteger();

					if (!(left <= right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_INTEGER_GREATER:
				{
					int offset = ReadShort();
					MidoriInteger right = Pop().GetInteger();
					MidoriInteger left = Pop().GetInteger();

					if (!(left > right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_INTEGER_GREATER_EQUAL:
				{
					int offset = ReadShort();
					MidoriInteger right = Pop().GetInteger();
					MidoriInteger left = Pop().GetInteger();

					if (!(left >= right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_INTEGER_EQUAL:
				{
					int offset = ReadShort();
					MidoriInteger right = Pop().GetInteger();
					MidoriInteger left = Pop().GetInteger();

					if (!(left == right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_INTEGER_NOT_EQUAL:
				{
					int offset = ReadShort();
					MidoriInteger right = Pop().GetInteger();
					MidoriInteger left = Pop().GetInteger();

					if (!(left != right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_FRACTION_LESS:
				{
					int offset = ReadShort();
					MidoriFraction right = Pop().GetFraction();
					MidoriFraction left = Pop().GetFraction();

					if (!(left < right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_FRACTION_LESS_EQUAL:
				{
					int offset = ReadShort();
					MidoriFraction right = Pop().GetFraction();
					MidoriFraction left = Pop().GetFraction();

					if (!(left <= right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_FRACTION_GREATER:
				{
					int offset = ReadShort();
					MidoriFraction right = Pop().GetFraction();
					MidoriFraction left = Pop().GetFraction();

					if (!(left > right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_FRACTION_GREATER_EQUAL:
				{
					int offset = ReadShort();
					MidoriFraction right = Pop().GetFraction();
					MidoriFraction left = Pop().GetFraction();

					if (!(left >= right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_FRACTION_EQUAL:
				{
					int offset = ReadShort();
					MidoriFraction right = Pop().GetFraction();
					MidoriFraction left = Pop().GetFraction();

					if (!(left == right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::IF_FRACTION_NOT_EQUAL:
				{
					int offset = ReadShort();
					MidoriFraction right = Pop().GetFraction();
					MidoriFraction left = Pop().GetFraction();

					if (!(left != right))
					{
						m_instruction_pointer += offset;
					}
					break;
				}
				case OpCode::LOAD_TAG:
				{
					MidoriValue& union_val = Pop();
					MidoriUnion& union_ref = union_val.GetPointer()->GetUnion();

					for (int i = 0; i < union_ref.m_values.GetLength(); i += 1)
					{
						Push(union_ref.m_values[i]);
					}

					Push(static_cast<MidoriInteger>(union_ref.m_index));
					break;
				}
				case OpCode::SET_TAG:
				{
					int tag = static_cast<int>(ReadByte());
					MidoriUnion& union_ref = Peek().GetPointer()->GetUnion();
					union_ref.m_index = tag;
					break;
				}
				case OpCode::CALL_FOREIGN:
				{
					const MidoriValue& foreign_function_name = Pop();
					int arity = static_cast<int>(ReadByte());
					MidoriText& foreign_function_name_ref = foreign_function_name.GetPointer()->GetText();

					// Platform-specific function loading
#ifdef _WIN32
					FARPROC proc = GetProcAddress(m_library_handle, foreign_function_name_ref.GetCString());
#else
					void* proc = dlsym(m_library_handle, foreign_function_name_ref.c_str());
#endif
					if (proc == nullptr)
					{
						TerminateExecution(GenerateRuntimeError(std::format("Failed to load foreign function '{}'.", foreign_function_name_ref.GetCString()), GetLine()));
					}

					MidoriValue args[UINT8_MAX];
					for (int i = arity - 1; i >= 0; i -= 1)
					{
						args[i] = Pop();
					}

					MidoriValue return_val;

					void(*ffi)(MidoriValue*, MidoriValue*) = reinterpret_cast<void(*)(MidoriValue*, MidoriValue*)>(proc);
					if (ffi == nullptr) [[unlikely]]
						{
							TerminateExecution(GenerateRuntimeError(std::format("Failed to load foreign function '{}'.", foreign_function_name_ref.GetCString()), GetLine()));
						}
					else [[likely]]
						{
							ffi(args, &return_val);
							Push(return_val);
						}

						break;
				}
				case OpCode::CALL_DEFINED:
				{
					const MidoriValue& callable = Pop();
					int arity = static_cast<int>(ReadByte());

					// Return address := pop all the arguments and the callee
					PushCallFrame(m_value_stack_base_pointer, m_value_stack_pointer - arity, m_instruction_pointer, callable.GetPointer());

					MidoriClosure& closure = callable.GetPointer()->GetClosure();
					m_curr_environment = &closure.m_cell_values;

					m_instruction_pointer = m_executable.GetBytecodeStream(closure.m_proc_index)[0];
					m_value_stack_base_pointer = m_value_stack_pointer - arity;

					break;
				}
				case OpCode::CONSTRUCT_STRUCT:
				{
					MidoriTraceable* new_struct = MidoriTraceable::AllocateTraceable(MidoriStruct());
					int size = static_cast<int>(ReadByte());
					MidoriArray args(size);

					for (int i = size - 1; i >= 0; i -= 1)
					{
						args[i] = Pop();
					}

					MidoriArray& members = new_struct->GetStruct().m_values;
					members = std::move(args);

					Push(new_struct);
					CollectGarbage();
					break;
				}
				case OpCode::CONSTRUCT_UNION:
				{
					MidoriTraceable* new_union = MidoriTraceable::AllocateTraceable(MidoriUnion());

					int size = static_cast<int>(ReadByte());
					MidoriArray args(size);

					for (int i = size - 1; i >= 0; i -= 1)
					{
						args[i] = Pop();
					}

					MidoriArray& members = new_union->GetUnion().m_values;
					members = std::move(args);

					Push(new_union);
					CollectGarbage();
					break;
				}
				case OpCode::ALLOCATE_CLOSURE:
				{
					int proc_index = static_cast<int>(ReadByte());
					Push(MidoriTraceable::AllocateTraceable(MidoriClosure{ MidoriClosure::Environment{}, proc_index }));
					CollectGarbage();
					break;
				}
				case OpCode::CONSTRUCT_CLOSURE:
				{
					int captured_count = static_cast<int>(ReadByte());

					if (captured_count == 0)
					{
						break;
					}

					MidoriClosure::Environment& captured_variables = (m_value_stack_pointer - 1)->GetPointer()->GetClosure().m_cell_values;

					const MidoriClosure::Environment& parent_closure = (m_call_stack_pointer - 1)->m_closure->GetClosure().m_cell_values;
					captured_variables = parent_closure;
					captured_count -= parent_closure.GetLength();

					std::for_each_n
					(
						std::execution::seq,
						m_value_stack_base_pointer,
						captured_count,
						[&captured_variables, this](MidoriValue& value)
						{
							MidoriValue* stack_value_ref = &value;
							MidoriValue cell_value = MidoriTraceable::AllocateTraceable(MidoriCellValue{ MidoriValue(), stack_value_ref, false });
							captured_variables.Add(cell_value);
							m_cells_to_promote[m_cell_promotion_count++] = &cell_value.GetPointer()->GetCellValue();
						}
					);
					CollectGarbage();
					break;
				}
				case OpCode::DEFINE_GLOBAL:
				{
					const MidoriValue& value = Pop();
					int global_idx = ReadGlobalVariable();
					MidoriValue& var = m_global_vars[global_idx];
					var = value;
					break;
				}
				case OpCode::GET_GLOBAL:
				{
					int global_idx = ReadGlobalVariable();
					Push(m_global_vars[global_idx]);
					break;
				}
				case OpCode::SET_GLOBAL:
				{
					int global_idx = ReadGlobalVariable();
					MidoriValue& var = m_global_vars[global_idx];
					var = Peek();
					break;
				}
				case OpCode::GET_LOCAL:
				{
					int offset = static_cast<int>(ReadByte());
					Push(*(m_value_stack_base_pointer + offset));
					break;
				}
				case OpCode::SET_LOCAL:
				{
					int offset = static_cast<int>(ReadByte());
					MidoriValue& var = *(m_value_stack_base_pointer + offset);

					const MidoriValue& value = Peek();
					var = value;
					break;
				}
				case OpCode::GET_CELL:
				{
					int offset = static_cast<int>(ReadByte());
					const MidoriValue& cell_value = (*m_curr_environment)[offset].GetPointer()->GetCellValue().GetValue();
					Push(cell_value);
					break;
				}
				case OpCode::SET_CELL:
				{
					int offset = static_cast<int>(ReadByte());
					MidoriValue& cell_value = (*m_curr_environment)[offset].GetPointer()->GetCellValue().GetValue();
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
				case OpCode::DUP:
				{
					Push(Peek());
					break;
				}
				case OpCode::POP_SCOPE:
				{
					// on scope exit, promote all cells to heap
					PromoteCells();

					m_value_stack_pointer -= static_cast<int>(ReadByte());
					break;
				}
				case OpCode::POP_MULTIPLE:
				{
					m_value_stack_pointer -= static_cast<int>(ReadByte());
					break;
				}
				case OpCode::RETURN:
				{
					// on return, promote all cells to heap
					PromoteCells();

					const MidoriValue& value = Pop();
					--m_call_stack_pointer;

					CallFrame& top_frame = *m_call_stack_pointer;

					m_curr_environment = &(m_call_stack_pointer - 1)->m_closure->GetClosure().m_cell_values;
					m_value_stack_base_pointer = top_frame.m_return_bp;
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
#ifdef _MSC_VER
					__assume(0);
#else
					__builtin_unreachable();
#endif
				}
				}
			}
}