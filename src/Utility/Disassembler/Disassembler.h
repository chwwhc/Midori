#ifdef DEBUG
#pragma once

#include <string_view>

class MidoriExecutable;

namespace Disassembler
{
    void DisassembleBytecodeStream(const MidoriExecutable& executable, int proc_index, std::string_view proc_name);

    void DisassembleInstruction(const MidoriExecutable& executable, int proc_index, int& offset);
  
};
#endif
