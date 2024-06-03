#include "Common/Value/Value.h"

#include <chrono>
#include <fstream>

#if defined(_WIN32) || defined(_WIN64)
#define MIDORI_API __declspec(dllexport)
#else
#define MIDORI_API
#endif

extern "C"
{
	/**
	 * 
	 *	IO
	 * 
	 */
	MIDORI_API void Print(MidoriValue* args, MidoriValue* ret) noexcept
	{
		std::printf(args[0u].GetPointer()->GetText().GetCString());
		new (ret) MidoriValue();
	}

	MIDORI_API void OverwriteToFile(MidoriValue* args, MidoriValue* ret) noexcept
	{
		std::string_view file_name = args[0u].GetPointer()->GetText().GetCString();
		std::string_view text = args[1u].GetPointer()->GetText().GetCString();

		std::ofstream file(file_name.data(), std::ios::out | std::ios::binary);
		if (!file.is_open())
		{
			new (ret) MidoriValue(false);
		}
		else
		{
			file.write(text.data(), text.size());
			new (ret) MidoriValue(true);
		}
	}

	MIDORI_API void AppendToFile(MidoriValue* args, MidoriValue* ret) noexcept
	{
		std::string_view file_name = args[0u].GetPointer()->GetText().GetCString();
		std::string_view text = args[1u].GetPointer()->GetText().GetCString();
		std::ofstream file(file_name.data(), std::ios::out | std::ios::app | std::ios::binary);
		if (!file.is_open())
		{
			new (ret) MidoriValue(false);
		}
		else
		{
			file.write(text.data(), text.size());
			new (ret) MidoriValue(true);
		}
	}


	/**
	 * 
	 * Math
	 * 
	 */
	

	/**
	 * 
	 * DateTime
	 * 
	 */
	MIDORI_API void GetTime(MidoriValue*, MidoriValue* ret) noexcept
	{
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::chrono::time_point now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
		std::chrono::milliseconds value = now_ms.time_since_epoch();
		std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(value);
		new (ret) MidoriValue(static_cast<MidoriFraction>(duration.count()));
	}
}
