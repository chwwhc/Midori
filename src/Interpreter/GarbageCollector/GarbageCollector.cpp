#include "GarbageCollector.h"

#include <algorithm>
#include <ranges>

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
