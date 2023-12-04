#include "Common/Value/Value.h"

#include <cstdio>

extern "C"
{
	__declspec(dllexport) void Print(MidoriValue* val)
	{
		std::printf("%s", val->GetObjectPointer()->GetText().c_str());
	}
}
