#pragma once

class ExecutableModule;

namespace Disassembler
{
    void DisassembleBytecodeStream(const ExecutableModule& stream, const char* name);

    void DisassembleInstruction(const ExecutableModule& stream, int& offset);
    //static void PrintValue(const Value& value);
};