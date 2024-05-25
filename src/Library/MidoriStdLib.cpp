#include "Common/Value/Value.h"

#include <chrono>

#if defined(_WIN32) || defined(_WIN64)
#define MIDORI_API __declspec(dllexport)
#else
#define MIDORI_API
#endif

extern "C"
{
	MIDORI_API void Print(MidoriValue* args, MidoriValue* ret) noexcept
	{
		std::printf(args[0u].GetPointer()->GetText().GetCString());
		new (ret) MidoriValue();
	}

	MIDORI_API void GetTime(MidoriValue*, MidoriValue* ret) noexcept
	{
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::chrono::time_point now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
		std::chrono::milliseconds value = now_ms.time_since_epoch();
		std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(value);
		new (ret) MidoriValue(static_cast<MidoriFraction>(duration.count()));
	}
}
