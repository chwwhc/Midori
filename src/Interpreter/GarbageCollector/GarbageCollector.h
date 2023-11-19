#pragma once

#include "Common/Value/Value.h"

class GarbageCollector
{
private:
	MidoriTraceable::GarbageCollectionRoots m_constant_roots;

public:
	explicit GarbageCollector(MidoriTraceable::GarbageCollectionRoots&& roots) : m_constant_roots(std::move(roots)) {}

	inline void ReclaimMemory(MidoriTraceable::GarbageCollectionRoots&& roots)
	{
		Mark(std::move(roots));
		Sweep();
	}

private:

	void Mark(MidoriTraceable::GarbageCollectionRoots&& roots);

	void Sweep();
};
