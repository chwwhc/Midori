#include "GarbageCollector.h"

void GarbageCollector::Mark(Traceable::GarbageCollectionRoots&& roots)
{
	roots.insert(m_constant_roots.begin(), m_constant_roots.end());
	std::vector<Traceable*> worklist(roots.begin(), roots.end());

	while (!worklist.empty())
	{
		Traceable* object = worklist.back();
		worklist.pop_back();

		if (!object->m_is_marked)
		{
			object->m_is_marked = true;
			Traceable::GarbageCollectionRoots children = GetChildren(object);
			for (Traceable* child : children)
			{
				worklist.emplace_back(child);
			}
		}
	}
}

void GarbageCollector::Sweep()
{
	std::list<Traceable*>::iterator it = Traceable::s_objects.begin();
	while (it != Traceable::s_objects.end())
	{
		Traceable* traceable = *it;
		if (traceable->m_is_marked)
		{
			traceable->m_is_marked = false;
			++it;
		}
		else
		{
			it = Traceable::s_objects.erase(it);
			delete traceable;
		}
	}
}

Traceable::GarbageCollectionRoots GarbageCollector::GetChildren(Traceable*)
{
	//Traceable::GarbageCollectionRoots children;

	//Object* object = static_cast<Object*>(root);

	//return children;

	return {};
}
