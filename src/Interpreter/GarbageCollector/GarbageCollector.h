#pragma once

#include "Common/Value/Value.h"

class GarbageCollector
{
private:
	const MidoriTraceable::GarbageCollectionRoots& m_constant_roots;

public:
	explicit GarbageCollector(const MidoriTraceable::GarbageCollectionRoots& roots) : m_constant_roots(roots) {}

	void ReclaimMemory(MidoriTraceable::GarbageCollectionRoots&& roots);

	void CleanUp();

	void PrintMemoryTelemetry();

private:
	void Mark(MidoriTraceable::GarbageCollectionRoots&& roots);

	void Sweep();
};
