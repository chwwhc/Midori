#include "GarbageCollector.h"

#include <algorithm>

#ifdef DEBUG
#include <iostream>
#endif

void GarbageCollector::Mark(MidoriTraceable::GarbageCollectionRoots&& roots)
{
	roots.insert(m_constant_roots.begin(), m_constant_roots.end());

	std::for_each(roots.begin(), roots.end(), [](MidoriTraceable* ptr) 
		{ 
			ptr->Trace(); 
		});
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
			std::cout << "Deleting traceable pointer: " << traceable_ptr << '\n';
#endif

			it = MidoriTraceable::s_traceables.erase(it);
			delete traceable_ptr;
		}
	}
	return;
}
