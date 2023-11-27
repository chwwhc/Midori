#include "Value.h"

#include <algorithm>
#include <iostream>

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
				return "#";
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
				return arg;
			}
			else if constexpr (std::is_same_v<T, MidoriArray>)
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
			else if constexpr (std::is_same_v<T, NativeFunction>)
			{
				return "<native function " + std::string(arg.m_name) + ">";
			}
			else if constexpr (std::is_same_v<T, MidoriStruct>)
			{
				if (arg.m_values.empty())
				{
					return "Struct{}";
				}

				std::string struct_val = "Struct";
				struct_val.append("{");
				std::for_each(arg.m_values.cbegin(), arg.m_values.cend(), [&struct_val](const MidoriValue& value) -> void
					{
						struct_val.append(value.ToString());
						struct_val.append(", ");
					});
				struct_val.pop_back();
				struct_val.pop_back();
				struct_val.append("}");
				return struct_val;
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
	else if (IsStruct())
	{
		MidoriStruct& midori_struct = GetStruct();
		std::for_each(midori_struct.m_values.begin(), midori_struct.m_values.end(), [](MidoriValue& value) -> void
			{
				if (value.IsObjectPointer())
				{
					value.GetObjectPointer()->Trace();
				}
			});
	}
}