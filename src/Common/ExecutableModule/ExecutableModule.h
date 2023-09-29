#pragma once

#include <vector>
#include <unordered_map>

#include "Value/Value.h"
#include "ByteCode/ByteCode.h

class ExecutableModule
{
public:
	/**
	* @brief Reads a byte from the bytecode stream.
	*/
	inline OpCode ReadByteCode(int index) const { return m_bytecode[index]; }

	/**
	 * @brief Sets a byte in the bytecode stream.
	 */
	inline void SetByteCode(int index, OpCode byte) { m_bytecode[index] = byte; }

	/**
	 * @brief Adds a byte to the bytecode stream.
	 */
	inline void AddByteCode(OpCode byte, int line)
	{
		m_bytecode.push_back(byte);

		if (m_line_info.empty() || m_line_info.back().first != line)
		{
			m_line_info.emplace_back(line, 1);
		}
		else
		{
			m_line_info.back().second++;
		}
	}

	/**
	 * @brief Adds a value to the value stream.
	 */
	inline int AddConstant(Value&& value)
	{
		std::unordered_map<Value, int>::const_iterator it = m_constants_table.find(value);
		if (it == m_constants_table.cend())
		{
			int index = static_cast<int>(m_constants.size());
			m_constants_table[value] = index;
			m_constants.emplace_back(std::move(value));
			return index;
		}
		else
		{
			return it->second;
		}
	}

	/**
	 * @brief Returns the size of the bytecode stream as int.
	 */
	inline int GetByteCodeSize() const { return static_cast<int>(m_bytecode.size()); }

	/**
	 * @brief Checks if the bytecode stream is empty.
	 */
	inline bool IsByteCodeEmpty() const { return m_bytecode.empty(); }

	/**
	 * @ brief Gets the constant value at the given index.
	 */
	inline const Value& GetConstant(int index) const { return m_constants[static_cast<size_t>(index)]; }

	/**
	 * @brief Returns the line at the given index.
	 */
	inline int GetLine(int index) const { return m_line_info[static_cast<size_t>(index)].first; }

private:
	std::vector<OpCode> m_bytecode;
	std::vector<std::pair<int, int>> m_line_info; // Pair of line number and count of consecutive instructions
	std::vector<Value> m_constants;
	std::unordered_map<Value, int> m_constants_table;
};
