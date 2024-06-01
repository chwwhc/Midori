#include "Value.h"
#include "Common/Printer/Printer.h"

#include <algorithm>
#include <execution>
#include <ranges>

namespace
{
	MidoriText ConvertToQuotedText(const MidoriText& input)
	{
		MidoriText result("\"");

		for (int i = 0; i < input.GetLength(); i += 1)
		{
			char c = input[i];
			switch (c)
			{
			case '\n':
			{
				result.Append("\\n");
				break;
			}
			case '\t':
			{
				result.Append("\\t");
				break;
			}
			case '\r':
			{
				result.Append("\\r");
				break;
			}
			case '\\':
			{
				result.Append("\\\\");
				break;
			}
			case '\"':
			{
				result.Append("\\\"");
				break;
			}
			default:
			{
				result.Append(c);
			}
			}
		}

		result.Append('"');

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

MidoriFraction MidoriValue::GetFraction() const
{
	return m_value.m_fraction;
}

bool MidoriValue::IsFraction() const
{
	return m_type_tag == MidoriValueTypeTag::Fraction;
}

MidoriInteger MidoriValue::GetInteger() const
{
	return m_value.m_integer;
}

bool MidoriValue::IsInteger() const
{
	return m_type_tag == MidoriValueTypeTag::Integer;
}

MidoriUnit MidoriValue::GetUnit() const
{
	return m_value.m_unit;
}

bool MidoriValue::IsUnit() const
{
	return m_type_tag == MidoriValueTypeTag::Unit;
}

MidoriBool MidoriValue::GetBool() const
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

MidoriText MidoriValue::ToText() const
{
	switch (m_type_tag)
	{
	case MidoriValue::MidoriValueTypeTag::Fraction:
		return MidoriText::FromFraction(GetFraction());
	case MidoriValue::MidoriValueTypeTag::Integer:
		return MidoriText::FromInteger(GetInteger());
	case MidoriValue::MidoriValueTypeTag::Unit:
		return MidoriText("()");
	case MidoriValue::MidoriValueTypeTag::Bool:
		return GetBool() ? MidoriText("true") : MidoriText("false");
	case MidoriValue::MidoriValueTypeTag::Pointer:
		return GetPointer()->ToText();
	default:
		return MidoriText("Unknown MidoriValue"); // Unreachable
	}
}

MidoriTraceable::MidoriTraceable(MidoriText&& str) noexcept : m_value(std::move(str))
{}

MidoriTraceable::MidoriTraceable(MidoriArray&& array) noexcept : m_value(std::move(array))
{}

MidoriTraceable::MidoriTraceable(MidoriCellValue&& cell_value) noexcept : m_value(std::move(cell_value))
{}

MidoriTraceable::MidoriTraceable(MidoriClosure&& closure) noexcept : m_value(std::move(closure))
{}

MidoriTraceable::MidoriTraceable(MidoriStruct&& midori_struct)noexcept : m_value(std::move(midori_struct))
{}

MidoriTraceable::MidoriTraceable(MidoriUnion&& midori_union) noexcept : m_value(std::move(midori_union))
{}

MidoriText MidoriTraceable::ToText()
{
	return std::visit([](auto&& arg) -> MidoriText
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, MidoriText>)
			{
				return ConvertToQuotedText(arg);
			}
			else if constexpr (std::is_same_v<T, MidoriArray>)
			{
				if (arg.GetLength() == 0)
				{
					return MidoriText("[]");
				}

				MidoriText result("[");
				for (int idx : std::views::iota(0, arg.GetLength()))
				{
					result.Append(arg[idx].ToText()).Append(", ");
				}
				result.Pop().Append("]");
				return result;
			}
			else if constexpr (std::is_same_v<T, MidoriCellValue>)
			{
				if (arg.m_is_on_heap)
				{
					return MidoriText("Cell(").Append(arg.m_heap_value.ToText()).Append(")");
				}
				else
				{
					return MidoriText("Cell(").Append(arg.m_stack_value_ref->ToText()).Append(")");
				}
			}
			else if constexpr (std::is_same_v<T, MidoriClosure>)
			{
				char buffer[64];
				std::snprintf(buffer, sizeof(buffer), "<closure at: %p>", (void*)std::addressof(arg));

				return MidoriText(buffer);
			}
			else if constexpr (std::is_same_v<T, MidoriUnion>)
			{
				if (arg.m_values.GetLength() == 0)
				{
					return MidoriText("Union{}");
				}

				MidoriText union_val("Union{");
				union_val.Append("{");

				for (int idx : std::views::iota(0, arg.m_values.GetLength()))
				{
					union_val.Append(arg.m_values[idx].ToText()).Append(", ");
				}
				return union_val.Pop().Pop().Append("}");
			}
			else if constexpr (std::is_same_v<T, MidoriStruct>)
			{
				if (arg.m_values.GetLength() == 0)
				{
					return MidoriText("Struct{}");
				}

				MidoriText struct_val("Struct{");
				struct_val.Append("{");
				for (int idx : std::views::iota(0, arg.m_values.GetLength()))
				{
					struct_val.Append(arg.m_values[idx].ToText()).Append(", ");
				}
				return struct_val.Pop().Pop().Append("}");
			}
			else
			{
				return MidoriText("Unknown MidoriTraceable");
			}
		}, m_value);
}

MidoriValue& MidoriCellValue::GetValue()
{
	return m_is_on_heap ? m_heap_value : *m_stack_value_ref;
}

bool MidoriTraceable::IsText() const
{
	return std::holds_alternative<MidoriText>(m_value);
}

MidoriText& MidoriTraceable::GetText()
{
	return std::get<MidoriText>(m_value);
}

bool MidoriTraceable::IsArray() const
{
	return std::holds_alternative<MidoriArray>(m_value);
}

MidoriArray& MidoriTraceable::GetArray()
{
	return std::get<MidoriArray>(m_value);
}

bool MidoriTraceable::IsCellValue() const
{
	return std::holds_alternative<MidoriCellValue>(m_value);
}

MidoriCellValue& MidoriTraceable::GetCellValue()
{
	return std::get<MidoriCellValue>(m_value);
}

bool MidoriTraceable::IsClosure() const
{
	return std::holds_alternative<MidoriClosure>(m_value);
}

MidoriClosure& MidoriTraceable::GetClosure()
{
	return std::get<MidoriClosure>(m_value);
}

bool MidoriTraceable::IsStruct() const
{
	return std::holds_alternative<MidoriStruct>(m_value);
}

MidoriStruct& MidoriTraceable::GetStruct()
{
	return std::get<MidoriStruct>(m_value);
}

bool MidoriTraceable::IsUnion() const
{
	return std::holds_alternative<MidoriUnion>(m_value);
}

MidoriUnion& MidoriTraceable::GetUnion()
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

void MidoriTraceable::Trace()
{
	if (Marked())
	{
		return;
	}
#ifdef DEBUG
	Printer::Print<Printer::Color::GREEN>(std::format("Marking traceable pointer: {:p}, value: {}\n", static_cast<void*>(this), ToText().GetCString()));
#endif
	Mark();

	if (IsArray())
	{
		MidoriArray& arr = GetArray();
		for (int idx : std::views::iota(0, arr.GetLength()))
		{
			MidoriValue& value = arr[idx];
			if (value.IsPointer())
			{
				value.GetPointer()->Trace();
			}
		}
	}
	else if (IsClosure())
	{
		MidoriArray& cell_values = GetClosure().m_cell_values;
		for (int i = 0; i < cell_values.GetLength(); i += 1)
		{
			MidoriValue& val = cell_values[i];
			val.GetPointer()->Trace();
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
		MidoriArray& arr = GetStruct().m_values;
		for (int idx : std::views::iota(0, arr.GetLength()))
		{
			MidoriValue& value = arr[idx];
			if (value.IsPointer())
			{
				value.GetPointer()->Trace();
			}
		}
	}
	else if (IsUnion())
	{
		MidoriArray& arr = GetUnion().m_values;

		for (int idx : std::views::iota(0, arr.GetLength()))
		{
			MidoriValue& value = arr[idx];
			if (value.IsPointer())
			{
				value.GetPointer()->Trace();
			}
		}
	}
}

MidoriArray::MidoriArray(int size)
{
	m_size = size < 0 ? s_initial_capacity : size;
	m_end = m_size;

	m_data = static_cast<MidoriValue*>(std::malloc(static_cast<size_t>(size) * sizeof(MidoriValue)));
	if (!m_data)
	{
		throw std::bad_alloc();
	}
}

MidoriArray::MidoriArray(const MidoriArray& other) : m_size(other.m_size), m_end(other.m_end)
{
	m_data = static_cast<MidoriValue*>(std::malloc(static_cast<size_t>(other.m_size) * sizeof(MidoriValue)));
	if (!m_data)
	{
		throw std::bad_alloc();
	}
	std::memcpy(m_data, other.m_data, static_cast<size_t>(other.m_size) * sizeof(MidoriValue));
}

MidoriArray::MidoriArray(MidoriArray&& other) noexcept : m_data(other.m_data), m_size(other.m_size), m_end(other.m_end)
{
	other.m_data = nullptr;
	other.m_size = 0;
	other.m_end = 0;
}

MidoriArray& MidoriArray::operator=(const MidoriArray& other)
{
	if (this != &other)
	{
		MidoriValue* new_data = static_cast<MidoriValue*>(std::malloc(static_cast<size_t>(other.m_size) * sizeof(MidoriValue)));
		if (!new_data)
		{
			throw std::bad_alloc();
		}
		std::memcpy(new_data, other.m_data, static_cast<size_t>(other.m_size) * sizeof(MidoriValue));

		std::free(m_data);
		m_data = new_data;
		m_size = other.m_size;
		m_end = other.m_end;
	}
	return *this;
}

MidoriArray& MidoriArray::operator=(MidoriArray&& other) noexcept
{
	if (this != &other)
	{
		std::free(m_data);
		m_data = other.m_data;
		m_size = other.m_size;
		m_end = other.m_end;

		other.m_data = nullptr;
		other.m_size = 0;
		other.m_end = 0;
	}
	return *this;
}

MidoriArray::~MidoriArray()
{
	std::free(m_data);
}

MidoriValue& MidoriArray::operator[](int index)
{
	return m_data[static_cast<size_t>(index)];
}

void MidoriArray::Expand()
{
	size_t new_size = m_size == 0u
		? s_initial_capacity
		: static_cast<size_t>(m_size) * 2u;
	MidoriValue* new_data = static_cast<MidoriValue*>(std::realloc(m_data, new_size * sizeof(MidoriValue)));
	if (!new_data)
	{
		throw std::bad_alloc();
	}
	m_data = new_data;
	m_size = static_cast<int>(new_size);
}

void MidoriArray::Add(const MidoriValue& value)
{
	if (m_end >= m_size)
	{
		Expand();
	}

	m_data[m_end] = value;
	m_end += 1;
}

int MidoriArray::GetLength() const
{
	return m_end;
}

MidoriArray MidoriArray::Concatenate(const MidoriArray& a, const MidoriArray& b)
{
	MidoriArray result(a.GetLength() + b.GetLength());
	std::memcpy(result.m_data, a.m_data, static_cast<size_t>(a.GetLength()) * sizeof(MidoriValue));
	std::memcpy(result.m_data + a.GetLength(), b.m_data, static_cast<size_t>(b.GetLength()) * sizeof(MidoriValue));
	result.m_end = a.GetLength() + b.GetLength();
	return result;
}

MidoriText::MidoriText(const char* str) : m_size(static_cast<int>(std::strlen(str))), m_capacity(m_size + 1)
{
	m_data = static_cast<char*>(std::malloc(static_cast<size_t>(m_capacity)));
	if (!m_data)
	{
		throw std::bad_alloc();
	}
	std::memcpy(m_data, str, static_cast<size_t>(m_size + 1));
}

MidoriText::MidoriText(const MidoriText& other) : m_size(other.m_size), m_capacity(other.m_capacity)
{
	m_data = static_cast<char*>(std::malloc(m_capacity));
	if (!m_data)
	{
		throw std::bad_alloc();
	}
	std::memcpy(m_data, other.m_data, static_cast<size_t>(m_size + 1));
}

MidoriText::MidoriText(MidoriText&& other) noexcept : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity)
{
	other.m_data = nullptr;
	other.m_size = 0;
	other.m_capacity = 0;
}

MidoriText::MidoriText(int size) : m_size(size), m_capacity(size + 1)
{
	m_data = static_cast<char*>(std::malloc(m_capacity));
	if (!m_data)
	{
		throw std::bad_alloc();
	}
	m_data[size] = '\0';
}

MidoriText& MidoriText::operator=(const MidoriText& other)
{
	if (this != &other)
	{
		char* new_data = static_cast<char*>(std::malloc(other.m_capacity));
		if (!new_data)
		{
			throw std::bad_alloc();
		}
		std::memcpy(new_data, other.m_data, static_cast<size_t>(other.m_size + 1));

		std::free(m_data);
		m_data = new_data;
		m_size = other.m_size;
		m_capacity = other.m_capacity;
	}
	return *this;
}

MidoriText& MidoriText::operator=(MidoriText&& other) noexcept
{
	if (this != &other)
	{
		std::free(m_data);
		m_data = other.m_data;
		m_size = other.m_size;
		m_capacity = other.m_capacity;

		other.m_data = nullptr;
		other.m_size = 0;
		other.m_capacity = 0;
	}
	return *this;
}

MidoriText::~MidoriText()
{
	std::free(m_data);
}

int MidoriText::GetLength() const noexcept
{
	return m_size;
}

const char* MidoriText::GetCString() const noexcept
{
	return m_data;
}

MidoriText& MidoriText::Pop()
{
	if (m_size > 0)
	{
		m_data[m_size - 1] = '\0';
		m_size -= 1;
	}
	return *this;
}

MidoriText& MidoriText::Append(const char* str)
{
	int str_len = static_cast<int>(std::strlen(str));
	if (m_size + str_len + 1 > m_capacity)
	{
		Expand(m_size + str_len + 1);
	}
	std::memcpy(m_data + m_size, str, static_cast<size_t>(str_len + 1));
	m_size += str_len;
	return *this;
}

MidoriText& MidoriText::Append(char c)
{
	if (m_size + 2 > m_capacity)
	{
		Expand(m_size + 2);
	}
	m_data[m_size] = c;
	m_data[m_size + 1] = '\0';
	m_size += 1;
	return *this;
}

MidoriText& MidoriText::Append(const MidoriText& other)
{
	if (m_size + other.m_size + 1 > m_capacity)
	{
		Expand(m_size + other.m_size + 1);
	}
	std::memcpy(m_data + m_size, other.m_data, static_cast<size_t>(other.m_size + 1));
	m_size += other.m_size;
	return *this;
}

char MidoriText::operator[](int index) const
{
	return m_data[static_cast<size_t>(index)];
}

bool MidoriText::operator==(const MidoriText& other) const
{
	if (m_size != other.m_size)
	{
		return false;
	}
	else
	{
		for (int i = 0; i < m_size; i++)
		{
			if ((*this)[i] != other[i])
			{
				return false;
			}
		}
		return true;
	}
}

bool MidoriText::operator!=(const MidoriText& other) const
{
	return !(*this == other);
}

MidoriInteger MidoriText::ToInteger() const
{
	return std::atoll(m_data);
}

MidoriFraction MidoriText::ToFraction() const
{
	return std::atof(m_data);
}

MidoriText MidoriText::FromInteger(MidoriInteger value)
{
	// 21 characters is the maximum length of a 64-bit integer
	char buffer[21];
	std::snprintf(buffer, 21, "%lld", value);
	return MidoriText(buffer);
}

MidoriText MidoriText::FromFraction(MidoriFraction value)
{
	// 32 characters is the maximum length of a 64-bit floating point number
	char buffer[32];
	std::snprintf(buffer, 32, "%f", value);
	return MidoriText(buffer);
}

MidoriText MidoriText::Concatenate(const MidoriText& a, const MidoriText& b)
{
	MidoriText output(a.m_size + b.m_size + 1);
	std::memcpy(output.m_data, a.m_data, static_cast<size_t>(a.m_size));
	std::memcpy(output.m_data + a.m_size, b.m_data, static_cast<size_t>(b.m_size + 1));
	output.m_size = a.m_size + b.m_size;
	return output;
}

void MidoriText::Expand(int new_size)
{
	char* new_data = static_cast<char*>(std::realloc(m_data, static_cast<size_t>(new_size)));
	if (!new_data)
	{
		throw std::bad_alloc();
	}
	m_data = new_data;
	m_capacity = static_cast<int>(new_size);
}