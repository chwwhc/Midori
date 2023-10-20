#pragma once

#include "Common/Value/Value.h"

class GarbageCollector
{
private:
	Traceable::GarbageCollectionRoots m_constant_roots;

public:
	explicit GarbageCollector(Traceable::GarbageCollectionRoots&& roots) : m_constant_roots(roots) {}

	inline void ReclaimMemory(Traceable::GarbageCollectionRoots&& roots)
	{
		Mark(std::move(roots));
		Sweep();
	}

private:

	void Mark(Traceable::GarbageCollectionRoots&& roots);

	void Sweep();
};
