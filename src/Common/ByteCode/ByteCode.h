#pragma once

#include <cinttypes>

enum class OpCode : uint8_t
{
    // Constants and Literals
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_CONSTANT_LONG_LONG,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,

    // Arithmetic Operations
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
};
