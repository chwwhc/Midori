#include "Common/Value/Value.h"

#include <cstdio>

#if defined(_WIN32) || defined(_WIN64)
#define MIDORI_API __declspec(dllexport)
#else
#define MIDORI_API
#endif

extern "C"
{
	MIDORI_API void Print(const std::vector<MidoriValue*>& args, MidoriValue* ret)
	{
		std::printf("%s", args[0]->GetObjectPointer()->GetText().c_str());
		new (ret) MidoriValue();
	}
}
