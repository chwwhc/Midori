#pragma once

class BytecodeStream;
class GlobalVariableTable;
class StaticData;

namespace Disassembler
{
    void DisassembleBytecodeStream(const BytecodeStream& stream, const StaticData& static_data, const GlobalVariableTable& global_table, const char* name);

    void DisassembleInstruction(const BytecodeStream& stream, const StaticData& static_data, const GlobalVariableTable& global_table, int& offset);
  
};