#include "GarbageCollector.h"

#include <algorithm>

void GarbageCollector::Mark(Traceable::GarbageCollectionRoots&& roots)
{
	roots.insert(m_constant_roots.begin(), m_constant_roots.end());

	std::for_each(roots.begin(), roots.end(), [](Traceable* obj) { obj->Trace(); });
}

void GarbageCollector::Sweep()
{
	std::list<Traceable*>::iterator it = Traceable::s_objects.begin();
	while (it != Traceable::s_objects.end())
	{
		Traceable* obj = *it;
		if (obj->IsMarked())
		{
			obj->Unmark();
			++it;
		}
		else
		{
			it = Traceable::s_objects.erase(it);
			delete obj;
		}
	}
}
