#include <iostream>

    extern "C"
    {
        __declspec(dllexport) void Print()
        {
            std::cout << "Hello from the DLL!" << std::endl;
        }
    }
