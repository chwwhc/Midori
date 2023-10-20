#include "GarbageCollector.h"

void GarbageCollector::Mark(Traceable::GarbageCollectionRoots&& roots)
{
	roots.insert(m_constant_roots.begin(), m_constant_roots.end());

	for (Traceable* obj : roots)
	{
		obj->Trace();
	}
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
