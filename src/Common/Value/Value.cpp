#include "Value.h"

#include <algorithm>
#include <iostream>

std::string MidoriValue::DoubleToStringWithoutTrailingZeros(double value)
{
	std::string str = std::to_string(value);

	size_t decimal_point = str.find('.');
	if (decimal_point != std::string::npos)
	{
		str.erase(str.find_last_not_of('0') + 1, std::string::npos);
		if (str.back() == '.')
		{
			str.pop_back();
		}
	}

	return str;
}

std::string MidoriValue::ToString() const
{
	return std::visit([](auto&& arg) -> std::string
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, double>)
			{
				return DoubleToStringWithoutTrailingZeros(arg);
			}
			else if constexpr (std::is_same_v<T, std::monostate>)
			{
				return "#";
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				return arg ? "true" : "false";
			}
			else if constexpr (std::is_same_v<T, NativeFunction>)
			{
				return "<native function " + std::string(arg.m_name) + ">";
			}
			else if constexpr (std::is_same_v<T, Traceable*>)
			{
				return arg->ToString();
			}
			else
			{
				return "Unknown value";
			}
		}, m_value);
}

std::string Traceable::ToString() const
{
	return std::visit([](auto&& arg) -> std::string
		{
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::string>)
			{
				return arg;
			}
			else if constexpr (std::is_same_v<T, std::vector<MidoriValue>>)
			{
				std::string result = "[";
				for (const MidoriValue& value : arg)
				{
					result.append(value.ToString());
					result.append(",");
				}
				result.pop_back();
				result.append("]");
				return result;
			}
			else if constexpr (std::is_same_v<T, CellValue>)
			{
				return "Cell(" + arg.m_value.ToString() + ")";
			}
			else if constexpr (std::is_same_v<T, Closure>)
			{
				return "<closure at: " + std::to_string(reinterpret_cast<uintptr_t>(&arg)) + ">";
			}
			else
			{
				return "Unknown Traceable";
			}
		}, m_value);
}

void Traceable::CleanUp()
{
	s_static_bytes_allocated = 0u;
	std::for_each(s_objects.begin(), s_objects.end(), [](Traceable* object) { delete object; });
	s_objects.clear();
}

void Traceable::PrintMemoryTelemetry()
{
	std::cout << "\n\t------------------------------\n";
	std::cout << "\tMemory telemetry:\n";
	std::cout << "\tHeap objects allocated: " << std::dec << s_objects.size() << '\n';
	std::cout << "\tTotal Bytes allocated: " << std::dec << s_total_bytes_allocated << '\n';
	std::cout << "\tStatic Bytes allocated: " << std::dec << s_static_bytes_allocated << '\n';
	std::cout << "\tDynamic Bytes allocated: " << std::dec << s_total_bytes_allocated - s_static_bytes_allocated;
	std::cout << "\n\t------------------------------\n\n";
}

void Traceable::Trace()
{
	if (IsMarked())
	{
		return;
	}
	Mark();
	if (IsArray())
	{
		std::for_each(GetArray().begin(), GetArray().end(), [](MidoriValue& value) -> void
			{
				if (value.IsObjectPointer())
				{
					value.GetObjectPointer()->Trace();
				}
			});
	}
	else if (IsClosure())
	{
		Traceable::Closure& closure = GetClosure();

		std::for_each(closure.m_cell_values.begin(), closure.m_cell_values.end(), [](Traceable* captured) -> void
			{
				if (captured != nullptr)
				{
					captured->Trace();
				}
			});
	}
	else if (IsCellValue())
	{
		MidoriValue& cellValue = GetCellValue().m_value;
		if (cellValue.IsObjectPointer())
		{
			cellValue.GetObjectPointer()->Trace();
		}
	}
}