#include "GarbageCollector.h"

#include <algorithm>
#include <ranges>
#include <execution>

#ifdef DEBUG
#include <format>
#include "Common\Printer\Printer.h"
#endif

void GarbageCollector::Mark(MidoriTraceable::GarbageCollectionRoots&& roots)
{
#ifdef DEBUG
	std::ranges::for_each
	(
		MidoriTraceable::s_traceables,
		[](MidoriTraceable* ptr)
		{
			Printer::Print<Printer::Color::CYAN>(std::format("Tracked traceable pointer: {:p}, value: {}\n", static_cast<void*>(ptr), ptr->ToText().GetCString()));
		}
	);
#endif
	roots.insert(m_constant_roots.begin(), m_constant_roots.end());
	std::ranges::for_each
	(
		roots,
		[](MidoriTraceable* ptr)
		{
			ptr->Trace();
		}
	);
}

void GarbageCollector::Sweep()
{
	std::list<MidoriTraceable*>::iterator it = MidoriTraceable::s_traceables.begin();
	while (it != MidoriTraceable::s_traceables.end())
	{
		MidoriTraceable* traceable_ptr = *it;
		if (traceable_ptr->Marked())
		{
			traceable_ptr->Unmark();
			++it;
		}
		else
		{
#ifdef DEBUG
			Printer::Print<Printer::Color::RED>(std::format("Deleting traceable pointer: {:p}\n", static_cast<void*>(traceable_ptr)));
#endif

			it = MidoriTraceable::s_traceables.erase(it);
			delete traceable_ptr;
		}
	}
	return;
}

void GarbageCollector::ReclaimMemory(MidoriTraceable::GarbageCollectionRoots&& roots)
{
	Mark(std::move(roots));
	Sweep();
}

void GarbageCollector::CleanUp()
{
#ifdef DEBUG
	Printer::Print<Printer::Color::BLUE>("\nBefore the final clean-up:\n");
	PrintMemoryTelemetry();
	std::ranges::for_each
	(
		MidoriTraceable::s_traceables,
		[](MidoriTraceable* traceable_ptr)
		{
			Printer::Print<Printer::Color::MAGENTA>(std::format("Deleting traceable pointer: {:p}\n", static_cast<void*>(traceable_ptr)));
			delete traceable_ptr;
		}
	);
#else
	std::for_each
	(
		std::execution::par_unseq,
		MidoriTraceable::s_traceables.begin(),
		MidoriTraceable::s_traceables.end(),
		[](MidoriTraceable* traceable_ptr)
		{
			delete traceable_ptr;
		}
	);
#endif
	MidoriTraceable::s_traceables.clear();
	MidoriTraceable::s_static_bytes_allocated = 0u;
#ifdef DEBUG
	Printer::Print<Printer::Color::BLUE>("\nAfter the final clean-up:\n");
	PrintMemoryTelemetry();
#endif
}

void GarbageCollector::PrintMemoryTelemetry()
{
#ifdef DEBUG
	Printer::Print<Printer::Color::BLUE>
		(
			std::format
			(
				"\n\t------------------------------\n"
				"\tMemory telemetry:\n"
				"\tHeap pointers allocated: {}\n"
				"\tTotal Bytes allocated: {}\n"
				"\tStatic Bytes allocated: {}\n"
				"\tDynamic Bytes allocated: {}\n"
				"\t------------------------------\n\n",
				MidoriTraceable::s_traceables.size(),
				MidoriTraceable::s_total_bytes_allocated,
				MidoriTraceable::s_static_bytes_allocated,
				MidoriTraceable::s_total_bytes_allocated - MidoriTraceable::s_static_bytes_allocated
			)
		);
#endif
}
