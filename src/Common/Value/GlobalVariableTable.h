#pragma once

#include <vector>
#include <string>

class GlobalVariableTable
{
private:
	std::vector<std::string> m_globals;

public:

	inline int AddGlobalVariable(std::string&& name)
	{
		m_globals.emplace_back(std::move(name));
		return static_cast<int>(m_globals.size()) - 1;
	}

	inline const std::string& GetGlobalVariable(int index) const { return m_globals[static_cast<size_t>(index)]; }

};
