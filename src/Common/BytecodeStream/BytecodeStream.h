#pragma once

#include <cinttypes>
#include <vector>

enum class OpCode : uint8_t
{
    // Constants and Literals
    CONSTANT,
    CONSTANT_LONG,
    CONSTANT_LONG_LONG,
    OP_UNIT,
    OP_TRUE,
    OP_FALSE,

    // Array Operations
    CREATE_ARRAY,
    GET_ARRAY,
    SET_ARRAY,

    // Bit Operations
    LEFT_SHIFT,
    RIGHT_SHIFT,
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR,
    BITWISE_NOT,

    // Arithmetic Operations
    ADD_FRACTION,
    SUBTRACT_FRACTION,
    MULTIPLY_FRACTION,
    DIVIDE_FRACTION,
    MODULO_FRACTION,
    ADD_INTEGER,
    SUBTRACT_INTEGER,
    MULTIPLY_INTEGER,
    DIVIDE_INTEGER,
    MODULO_INTEGER,

    // Aggregate Operations
    CONCAT_ARRAY,
    CONCAT_TEXT,

    // Comparison Operations
    EQUAL_FRACTION,
    NOT_EQUAL_FRACTION,
    GREATER_FRACTION,
    GREATER_EQUAL_FRACTION,
    LESS_FRACTION,
    LESS_EQUAL_FRACTION,
    EQUAL_INTEGER,
    NOT_EQUAL_INTEGER,
    GREATER_INTEGER,
    GREATER_EQUAL_INTEGER,
    LESS_INTEGER,
    LESS_EQUAL_INTEGER,

    // Logical Operations
    NOT,

    // Unary Operations
    NEGATE_FRACTION,
    NEGATE_INTEGER,

    // Control Flow
    JUMP_IF_FALSE,
    JUMP_IF_TRUE,
    JUMP,
    JUMP_BACK,

    // Callable
    CALL_NATIVE,
    CALL_DEFINED,
    CONSTRUCT_STRUCT,
    ALLOCATE_STRUCT,

    // Variable Operations
    CREATE_CLOSURE,
    DEFINE_GLOBAL,
    GET_GLOBAL,
    SET_GLOBAL,
    GET_LOCAL,
    SET_LOCAL,
    GET_CELL,
    SET_CELL,

    // Struct Operations
    GET_MEMBER,
    SET_MEMBER,

    // Stack Operations
    POP,
    POP_MULTIPLE,

    // Return
    RETURN,
    HALT,
};


class BytecodeStream
{
public:
    using iterator = std::vector<OpCode>::iterator;
    using const_iterator = std::vector<OpCode>::const_iterator;

    inline iterator begin() { return m_bytecode.begin(); }
    inline iterator end() { return m_bytecode.end(); }
    inline const_iterator begin() const { return m_bytecode.begin(); }
    inline const_iterator end() const { return m_bytecode.end(); }
    inline const_iterator cbegin() const { return m_bytecode.cbegin(); }
    inline const_iterator cend() const { return m_bytecode.cend(); }

private:
	std::vector<OpCode> m_bytecode;
	std::vector<std::pair<int, int>> m_line_info; // Pair of line number and count of consecutive instructions

public:

	inline OpCode ReadByteCode(int index) const { return m_bytecode[static_cast<size_t>(index)]; }

	inline void SetByteCode(int index, OpCode byte) { m_bytecode[static_cast<size_t>(index)] = byte; }

	inline void AddByteCode(OpCode byte, int line)
	{
		m_bytecode.push_back(byte);

		if (m_line_info.empty() || m_line_info.back().first != line)
		{
			m_line_info.emplace_back(line, 1);
		}
		else
		{
            m_line_info.back().second += 1;
		}
	}

	inline int GetByteCodeSize() const { return static_cast<int>(m_bytecode.size()); }

	inline bool IsByteCodeEmpty() const { return m_bytecode.empty(); }

    inline int GetLine(int index) const
    {
        int cumulative_count = 0;
        for (const auto& [line, count] : m_line_info)
        {
            cumulative_count += count;
            if (index < cumulative_count)
            {
                return line;
            }
        }

        return -1; // TODO: Throw exception
    }
};
