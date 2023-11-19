#include "Value.h"

#include <algorithm>
#include <iostream>

std::string MidoriValue::DoubleToStringWithoutTrailingZeros(MidoriFraction value)
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

			if constexpr (std::is_same_v<T, MidoriFraction>)
			{
				return DoubleToStringWithoutTrailingZeros(arg);
			}
			else if constexpr (std::is_same_v<T, MidoriInteger>)
			{
				return std::to_string(arg);
			}
			else if constexpr (std::is_same_v<T, MidoriUnit>)
			{
				return "#";
			}
			else if constexpr (std::is_same_v<T, MidoriBool>)
			{
				return arg ? "true" : "false";
			}
			else if constexpr (std::is_same_v<T, NativeFunction>)
			{
				return "<native function " + std::string(arg.m_name) + ">";
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
			if constexpr (std::is_same_v<T, std::string>)
			{
				return arg;
			}
			else if constexpr (std::is_same_v<T, std::vector<MidoriValue>>)
			{
				std::string result = "[";
				std::for_each(arg.begin(), arg.end(), [&result](const MidoriValue& value) -> void
					{
						result.append(value.ToString());
						result.append(",");
					});
				result.pop_back();
				result.append("]");
				return result;
			}
			else if constexpr (std::is_same_v<T, CellValue>)
			{
				return "Cell(" + arg.ToString() + ")";
			}
			else if constexpr (std::is_same_v<T, Closure>)
			{
				return "<closure at: " + std::to_string(reinterpret_cast<uintptr_t>(&arg)) + ">";
			}
			else
			{
				return "Unknown MidoriTraceable";
			}
		}, m_value);
}

void MidoriTraceable::CleanUp()
{
	s_static_bytes_allocated = 0u;
	std::for_each(s_objects.begin(), s_objects.end(), [](MidoriTraceable* object) { delete object; });
	s_objects.clear();
}

void MidoriTraceable::PrintMemoryTelemetry()
{
	std::cout << "\n\t------------------------------\n";
	std::cout << "\tMemory telemetry:\n";
	std::cout << "\tHeap objects allocated: " << std::dec << s_objects.size() << '\n';
	std::cout << "\tTotal Bytes allocated: " << std::dec << s_total_bytes_allocated << '\n';
	std::cout << "\tStatic Bytes allocated: " << std::dec << s_static_bytes_allocated << '\n';
	std::cout << "\tDynamic Bytes allocated: " << std::dec << s_total_bytes_allocated - s_static_bytes_allocated;
	std::cout << "\n\t------------------------------\n\n";
}

void MidoriTraceable::Trace()
{
	if (Marked())
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
		MidoriTraceable::Closure& closure = GetClosure();

		std::for_each(closure.m_cell_values.begin(), closure.m_cell_values.end(), [](MidoriTraceable* captured) -> void
			{
				if (captured != nullptr)
				{
					captured->Trace();
				}
			});
	}
	else if (IsCellValue())
	{
		MidoriValue& cell_value = GetCellValue();
		if (cell_value.IsObjectPointer())
		{
			cell_value.GetObjectPointer()->Trace();
		}
	}
}