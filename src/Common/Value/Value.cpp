#include "Value.h"
#include "Common/Printer/Printer.h"

#include <algorithm>
#include <ranges>
#include <format>

using namespace std::string_literals;

namespace
{
	std::string ConvertToQuotedString(std::string_view input)
	{
		std::string result = "\"";

		for (char c : input)
		{
			switch (c) {
			case '\n':
			{
				result.append("\\n");
				break;
			}
			case '\t':
			{
				result.append("\\t");
				break;
			}
			case '\r':
			{
				result.append("\\r");
				break;
			}
			case '\\':
			{
				result.append("\\\\");
				break;
			}
			case '\"':
			{
				result.append("\\\"");
				break;
			}
			default:
			{
				result.push_back(c);
			}
			}
		}

		result.append("\"");

		return result;
	}
}

MidoriValue::MidoriFraction MidoriValue::GetFraction() const
{
	return std::get<MidoriFraction>(m_value);
}

bool MidoriValue::IsFraction() const
{
	return std::holds_alternative<MidoriFraction>(m_value);
}

MidoriValue::MidoriInteger MidoriValue::GetInteger() const
{
	return std::get<MidoriInteger>(m_value);
}

bool MidoriValue::IsInteger() const
{
	return std::holds_alternative<MidoriInteger>(m_value);
}

MidoriValue::MidoriUnit MidoriValue::GetUnit() const
{
	return std::get<MidoriUnit>(m_value);
}

bool MidoriValue::IsUnit() const
{
	return std::holds_alternative<MidoriUnit>(m_value);
}

MidoriValue::MidoriBool MidoriValue::GetBool() const
{
	return std::get<MidoriBool>(m_value);
}

bool MidoriValue::IsBool() const
{
	return std::holds_alternative<MidoriBool>(m_value);
}

MidoriTraceable* MidoriValue::GetPointer() const
{
	return std::get<MidoriTraceable*>(m_value);
}

bool MidoriValue::IsPointer() const
{
	return std::holds_alternative<MidoriTraceable*>(m_value);
}

std::string MidoriValue::ToString() const
{
	return std::visit([](auto&& arg) -> std::string
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, MidoriFraction>)
			{
				return std::to_string(arg);
			}
			else if constexpr (std::is_same_v<T, MidoriInteger>)
			{
				return std::to_string(arg);
			}
			else if constexpr (std::is_same_v<T, MidoriUnit>)
			{
				return "()";
			}
			else if constexpr (std::is_same_v<T, MidoriBool>)
			{
				return arg ? "true" : "false";
			}
			else if constexpr (std::is_same_v<T, MidoriTraceable*>)
			{
				return arg->ToString();
			}
			else
			{
				return "Unknown value";
			}
		}, m_value);
}

std::string MidoriTraceable::ToString() const
{
	return std::visit([](auto&& arg) -> std::string
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, MidoriText>)
			{
				return ConvertToQuotedString(arg);
			}
			else if constexpr (std::is_same_v<T, MidoriArray>)
			{
				if (arg.empty())
				{
					return "[]";
				}

				std::string result = "["s;
				std::ranges::for_each(arg, [&result](const MidoriValue& value) -> void
					{
						result.append(value.ToString());
						result.append(","s);
					});
				result.pop_back();
				result.append("]"s);
				return result;
			}
			else if constexpr (std::is_same_v<T, CellValue>)
			{
				if (arg.m_is_on_heap)
				{
					return "Cell("s + arg.m_heap_value.ToString() + ")"s;
				}
				else
				{
					return "Cell("s + arg.m_stack_value_ref->ToString() + ")"s;
				}
			}
			else if constexpr (std::is_same_v<T, MidoriClosure>)
			{
				return std::format("<closure at: {:p}>", (void*)std::addressof(arg));
			}
			else if constexpr (std::is_same_v<T, MidoriUnion>)
			{
				if (arg.m_values.empty())
				{
					return "Union{}"s;
				}

				std::string union_val = "Union"s;
				union_val.append("{"s);
				std::ranges::for_each(arg.m_values, [&union_val](const MidoriValue& value) -> void
					{
						union_val.append(value.ToString());
						union_val.append(", "s);
					});
				union_val.pop_back();
				union_val.pop_back();
				union_val.append("}"s);
				return union_val;
			}
			else if constexpr (std::is_same_v<T, MidoriStruct>)
			{
				if (arg.m_values.empty())
				{
					return "Struct{}"s;
				}

				std::string struct_val = "Struct"s;
				struct_val.append("{"s);
				std::ranges::for_each(arg.m_values, [&struct_val](const MidoriValue& value) -> void
					{
						struct_val.append(value.ToString());
						struct_val.append(", "s);
					});
				struct_val.pop_back();
				struct_val.pop_back();
				struct_val.append("}"s);
				return struct_val;
			}
			else
			{
				return "Unknown MidoriTraceable"s;
			}
		}, m_value);
}

MidoriValue& MidoriTraceable::CellValue::GetValue()
{
	return m_is_on_heap ? m_heap_value : *m_stack_value_ref;
}

MidoriTraceable::MidoriText& MidoriTraceable::GetText() const
{
	return const_cast<MidoriText&>(std::get<MidoriText>(m_value));
}

bool MidoriTraceable::IsText() const
{
	return std::holds_alternative<MidoriText>(m_value);
}

MidoriTraceable::MidoriArray& MidoriTraceable::GetArray() const
{
	return const_cast<MidoriArray&>(std::get<MidoriArray>(m_value));
}

bool MidoriTraceable::IsArray() const
{
	return std::holds_alternative<MidoriArray>(m_value);
}

bool MidoriTraceable::IsCellValue() const
{
	return std::holds_alternative<CellValue>(m_value);
}

MidoriTraceable::CellValue& MidoriTraceable::GetCellValue() const
{
	return const_cast<CellValue&>(std::get<CellValue>(m_value));
}

bool MidoriTraceable::IsClosure() const
{
	return std::holds_alternative<MidoriClosure>(m_value);
}

MidoriTraceable::MidoriClosure& MidoriTraceable::GetClosure() const
{
	return const_cast<MidoriClosure&>(std::get<MidoriClosure>(m_value));
}

bool MidoriTraceable::IsStruct() const
{
	return std::holds_alternative<MidoriStruct>(m_value);
}

MidoriTraceable::MidoriStruct& MidoriTraceable::GetStruct() const
{
	return const_cast<MidoriStruct&>(std::get<MidoriStruct>(m_value));
}

bool MidoriTraceable::IsUnion() const 
{
	return std::holds_alternative<MidoriUnion>(m_value);
}

MidoriTraceable::MidoriUnion& MidoriTraceable::GetUnion() const
{
	return const_cast<MidoriUnion&>(std::get<MidoriUnion>(m_value));
}

size_t MidoriTraceable::GetSize() const
{
	return m_size;
}

void MidoriTraceable::Mark()
{
	m_is_marked = true;
}

void MidoriTraceable::Unmark()
{
	m_is_marked = false;
}

bool MidoriTraceable::Marked() const
{
	return m_is_marked;
}

void* MidoriTraceable::operator new(size_t size) noexcept
{
	void* object = ::operator new(size);
	MidoriTraceable* traceable = static_cast<MidoriTraceable*>(object);

	traceable->m_size = size;
	s_total_bytes_allocated += size;
	s_traceables.emplace_back(traceable);

	return static_cast<void*>(traceable);
}

void MidoriTraceable::operator delete(void* object, size_t size) noexcept
{
	MidoriTraceable* traceable = static_cast<MidoriTraceable*>(object);
	s_total_bytes_allocated -= traceable->m_size;

	::operator delete(object, size);
}

void MidoriTraceable::CleanUp()
{
	s_static_bytes_allocated = 0u;
	std::ranges::for_each(s_traceables, [](MidoriTraceable* object) { delete object; });
	s_traceables.clear();
}

void MidoriTraceable::PrintMemoryTelemetry()
{
	Printer::Print<Printer::Color::BLUE>
		(
			std::format
			(
				"\n\t------------------------------\n"
				"\tMemory telemetry:\n"
				"\tHeap objects allocated: {}\n"
				"\tTotal Bytes allocated: {}\n"
				"\tStatic Bytes allocated: {}\n"
				"\tDynamic Bytes allocated: {}\n"
				"\t------------------------------\n\n",
				s_traceables.size(),
				s_total_bytes_allocated,
				s_static_bytes_allocated,
				s_total_bytes_allocated - s_static_bytes_allocated
			)
		);
}

void MidoriTraceable::Trace()
{
	if (Marked())
	{
		return;
	}
#ifdef DEBUG
	Printer::Print<Printer::Color::GREEN>(std::format("Marking traceable pointer: {:p}, value: {}\n", static_cast<void*>(this), ToString()));
#endif
	Mark();

	if (IsArray())
	{
		MidoriArray& array = GetArray();
		std::ranges::for_each(array, [](MidoriValue& value) -> void
			{
				if (value.IsPointer())
				{
					value.GetPointer()->Trace();
				}
			});
	}
	else if (IsClosure())
	{
		MidoriClosure& closure = GetClosure();
		std::ranges::for_each(closure.m_cell_values, [](MidoriValue& cell_value) -> void
			{
				if (cell_value.IsPointer())
				{
					cell_value.GetPointer()->Trace();
				}
			});
	}
	else if (IsCellValue())
	{
		MidoriValue& cell_value = GetCellValue().GetValue();
		if (cell_value.IsPointer())
		{
			cell_value.GetPointer()->Trace();
		}
	}
	else if (IsStruct())
	{
		MidoriStruct& midori_struct = GetStruct();
		std::ranges::for_each(midori_struct.m_values, [](MidoriValue& value) -> void
			{
				if (value.IsPointer())
				{
					value.GetPointer()->Trace();
				}
			});
	}
	else if (IsUnion())
	{
		MidoriUnion& midori_union = GetUnion();
		std::ranges::for_each(midori_union.m_values, [](MidoriValue& value) -> void
			{
				if (value.IsPointer())
				{
					value.GetPointer()->Trace();
				}
			});
	}
}