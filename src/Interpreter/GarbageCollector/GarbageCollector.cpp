#include "GarbageCollector.h"

#include <algorithm>

void GarbageCollector::Mark(MidoriTraceable::GarbageCollectionRoots&& roots)
{
	roots.insert(m_constant_roots.begin(), m_constant_roots.end());

	std::for_each(roots.begin(), roots.end(), [](MidoriTraceable* obj) { obj->Trace(); });
}

void GarbageCollector::Sweep()
{
	std::list<MidoriTraceable*>::iterator it = MidoriTraceable::s_objects.begin();
	while (it != MidoriTraceable::s_objects.end())
	{
		MidoriTraceable* obj = *it;
		if (obj->Marked())
		{
			obj->Unmark();
			++it;
		}
		else
		{
			it = MidoriTraceable::s_objects.erase(it);
			delete obj;
		}
	}
}
