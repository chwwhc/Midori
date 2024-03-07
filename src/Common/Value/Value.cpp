#include "Value.h"
#include "Common/Printer/Printer.h"

#include <algorithm>
#include <execution>
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
			switch (c)
			{
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

MidoriValue::MidoriValueUnion::MidoriValueUnion(MidoriFraction d) noexcept : m_fraction(d)
{}

MidoriValue::MidoriValueUnion::MidoriValueUnion(MidoriInteger l) noexcept : m_integer(l)
{}

MidoriValue::MidoriValueUnion::MidoriValueUnion(MidoriUnit u) noexcept : m_unit(u)
{}

MidoriValue::MidoriValueUnion::MidoriValueUnion(MidoriBool b) noexcept : m_bool(b)
{}

MidoriValue::MidoriValueUnion::MidoriValueUnion(MidoriTraceable* o) noexcept : m_pointer(o)
{}

MidoriValue::MidoriValue(MidoriFraction d) noexcept : m_value(d), m_type_tag(MidoriValueTypeTag::Fraction)
{}

MidoriValue::MidoriValue(MidoriInteger l) noexcept : m_value(l), m_type_tag(MidoriValueTypeTag::Integer)
{}

MidoriValue::MidoriValue(MidoriBool b) noexcept : m_value(b), m_type_tag(MidoriValueTypeTag::Bool)
{}

MidoriValue::MidoriValue(MidoriTraceable* o) noexcept : m_value(o), m_type_tag(MidoriValueTypeTag::Pointer)
{}

MidoriValue::MidoriFraction MidoriValue::GetFraction() const
{
	return m_value.m_fraction;
}

bool MidoriValue::IsFraction() const
{
	return m_type_tag == MidoriValueTypeTag::Fraction;
}

MidoriValue::MidoriInteger MidoriValue::GetInteger() const
{
	return m_value.m_integer;
}

bool MidoriValue::IsInteger() const
{
	return m_type_tag == MidoriValueTypeTag::Integer;
}

MidoriValue::MidoriUnit MidoriValue::GetUnit() const
{
	return m_value.m_unit;
}

bool MidoriValue::IsUnit() const
{
	return m_type_tag == MidoriValueTypeTag::Unit;
}

MidoriValue::MidoriBool MidoriValue::GetBool() const
{
	return m_value.m_bool;
}

bool MidoriValue::IsBool() const
{
	return m_type_tag == MidoriValueTypeTag::Bool;
}

MidoriTraceable* MidoriValue::GetPointer() const
{
	return m_value.m_pointer;
}

bool MidoriValue::IsPointer() const
{
	return m_type_tag == MidoriValueTypeTag::Pointer;
}

std::string MidoriValue::ToString() const
{
	switch (m_type_tag)
	{
	case MidoriValue::MidoriValueTypeTag::Fraction:
		return std::to_string(GetFraction());
	case MidoriValue::MidoriValueTypeTag::Integer:
		return std::to_string(GetInteger());
	case MidoriValue::MidoriValueTypeTag::Unit:
		return "()"s;
	case MidoriValue::MidoriValueTypeTag::Bool:
		return GetBool() ? "true"s : "false"s;
	case MidoriValue::MidoriValueTypeTag::Pointer:
		return GetPointer()->ToString();
	default:
		return "Unknown MidoriValue"s; // Unreachable
	}
}

MidoriTraceable::MidoriTraceable(MidoriText&& str) noexcept : m_value(std::move(str))
{}

MidoriTraceable::MidoriTraceable(MidoriArray&& array) noexcept : m_value(std::move(array))
{}

MidoriTraceable::MidoriTraceable(MidoriValue* stack_value_ref) noexcept : m_value(CellValue{ MidoriValue(), stack_value_ref, false })
{}

MidoriTraceable::MidoriTraceable(MidoriClosure&& closure) noexcept : m_value(std::move(closure))
{}

MidoriTraceable::MidoriTraceable(MidoriStruct&& midori_struct)noexcept : m_value(std::move(midori_struct))
{}

MidoriTraceable::MidoriTraceable(MidoriUnion&& midori_union) noexcept : m_value(std::move(midori_union))
{}

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
					return "[]"s;
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

bool MidoriTraceable::IsText() const
{
	return std::holds_alternative<MidoriText>(m_value);
}

MidoriTraceable::MidoriText& MidoriTraceable::GetText()
{
	return std::get<MidoriText>(m_value);
}

bool MidoriTraceable::IsArray() const
{
	return std::holds_alternative<MidoriArray>(m_value);
}

MidoriTraceable::MidoriArray& MidoriTraceable::GetArray()
{
	return std::get<MidoriArray>(m_value);
}

bool MidoriTraceable::IsCellValue() const
{
	return std::holds_alternative<CellValue>(m_value);
}

MidoriTraceable::CellValue& MidoriTraceable::GetCellValue()
{
	return std::get<CellValue>(m_value);
}

bool MidoriTraceable::IsClosure() const
{
	return std::holds_alternative<MidoriClosure>(m_value);
}

MidoriTraceable::MidoriClosure& MidoriTraceable::GetClosure()
{
	return std::get<MidoriClosure>(m_value);
}

bool MidoriTraceable::IsStruct() const
{
	return std::holds_alternative<MidoriStruct>(m_value);
}

MidoriTraceable::MidoriStruct& MidoriTraceable::GetStruct()
{
	return std::get<MidoriStruct>(m_value);
}

bool MidoriTraceable::IsUnion() const
{
	return std::holds_alternative<MidoriUnion>(m_value);
}

MidoriTraceable::MidoriUnion& MidoriTraceable::GetUnion()
{
	return std::get<MidoriUnion>(m_value);
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
#ifdef DEBUG
	Printer::Print<Printer::Color::BLUE>("\nBefore the final clean-up:\n");
	PrintMemoryTelemetry();
	std::ranges::for_each
	(
		s_traceables,
		[](MidoriTraceable* traceable_ptr)
		{
			Printer::Print<Printer::Color::MAGENTA>(std::format("Deleting traceable pointer: {:p}\n", static_cast<void*>(traceable_ptr)));
			delete traceable_ptr;
		}
	);
#else
	std::for_each
	(
		std::execution::par_unseq,
		s_traceables.begin(),
		s_traceables.end(),
		[](MidoriTraceable* traceable_ptr)
		{
			delete traceable_ptr;
		}
	);
#endif
	s_traceables.clear();
	s_static_bytes_allocated = 0u;
#ifdef DEBUG
	Printer::Print<Printer::Color::BLUE>("\nAfter the final clean-up:\n");
	PrintMemoryTelemetry();
#endif
}

void MidoriTraceable::PrintMemoryTelemetry()
{
	Printer::Print<Printer::Color::BLUE>
		(
			std::format
			(
				"\n\t------------------------------\n"
				"\tMemory telemetry:\n"
				"\tHeap pointers allocated: {}\n"
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
		for (auto& pointer : GetArray() | std::views::filter([](const auto& val)
			{
				return val.IsPointer();
			}))
		{
			pointer.GetPointer()->Trace();
		}
	}
	else if (IsClosure())
	{
		for (auto& pointer : GetClosure().m_cell_values | std::views::filter([](const auto& val)
			{
				return val.IsPointer();
			}))
		{
			pointer.GetPointer()->Trace();
		}
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
		for (auto& pointer : GetStruct().m_values | std::views::filter([](const auto& val)
			{
				return val.IsPointer();
			}))
		{
			pointer.GetPointer()->Trace();
		}
	}
	else if (IsUnion())
	{
		for (auto& pointer : GetUnion().m_values | std::views::filter([](const auto& val)
			{
				return val.IsPointer();
			}))
		{
			pointer.GetPointer()->Trace();
		}
	}
}