#pragma once

#include "Value.h"

class StaticData
{
private:
	std::vector<Value> m_constants;

public:

	inline const Value& GetConstant(int index) const { return m_constants[static_cast<size_t>(index)]; }

	inline int AddConstant(Value&& value)
	{
		m_constants.emplace_back(std::move(value));
		return static_cast<int>(m_constants.size()) - 1;
	}
};
