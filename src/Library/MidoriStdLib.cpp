#include "Common/Value/Value.h"
#include "Interpreter/VirtualMachine/VirtualMachine.h"

#include <print>
#include <chrono>
#include <span>
#include <memory>

#if defined(_WIN32) || defined(_WIN64)
#define MIDORI_API __declspec(dllexport)
#else
#define MIDORI_API
#endif

extern "C"
{
	MIDORI_API void Print(std::span<MidoriValue> args, MidoriValue* ret) noexcept
	{
		std::print("{}", args[0].GetPointer()->GetText());
		std::construct_at(ret);
	}

	MIDORI_API void GetTime(std::span<MidoriValue>, MidoriValue* ret) noexcept
	{
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::chrono::time_point now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
		std::chrono::milliseconds value = now_ms.time_since_epoch();
		std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(value);
		std::construct_at(ret, static_cast<MidoriValue::MidoriFraction>(duration.count()));
	}
}
