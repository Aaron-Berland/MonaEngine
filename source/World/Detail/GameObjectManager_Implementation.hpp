#pragma once
#ifndef GAMEOBJECTMANAGER_IMPLEMENTATION_HPP
#define GAMEOBJECTMANAGER_IMPLEMENTATION_HPP
#include <utility>
namespace Mona {

	template <typename ObjectType, typename ...Args>
	InnerGameObjectHandle GameObjectManager::CreateGameObject(World& world, Args&& ... args)
	{
		static_assert(std::is_base_of<GameObject, ObjectType>::value, "ObjectType must be a derived class from GameObject");
		MONA_ASSERT(m_gameObjects.size() < s_maxEntries, "GameObjectManager Error: Cannot Add more objects, max number reached.");
		auto gameObjectPointer = std::make_unique<ObjectType>(std::forward<Args>(args)...);
		if (m_firstFreeIndex != s_maxEntries && m_freeIndicesCount > s_minFreeIndices)
		{
			auto& handleEntry = m_handleEntries[m_firstFreeIndex];
			MONA_ASSERT(handleEntry.active == false, "GameObjectManager Error: Incorrect active state for handleEntry");
			MONA_ASSERT(handleEntry.generation < std::numeric_limits<decltype(handleEntry.generation)>::max(),
				"GameObjectManager Error: Generational Index reached its maximunn value, GameObject cannot be added.");
			auto handleIndex = m_firstFreeIndex;
			
			if (m_firstFreeIndex == m_lastFreeIndex)
				m_firstFreeIndex = m_lastFreeIndex = s_maxEntries;
			else
				m_firstFreeIndex = handleEntry.index;
			handleEntry.generation += 1;
			handleEntry.active = true;
			handleEntry.index = static_cast<size_type>(m_gameObjects.size());
			handleEntry.prevIndex = s_maxEntries;
			--m_freeIndicesCount;
			InnerGameObjectHandle resultHandle(handleIndex, handleEntry.generation);
			gameObjectPointer->SetObjectHandle(resultHandle);
			m_gameObjects.emplace_back(std::move(gameObjectPointer));
			m_gameObjectHandleIndices.emplace_back(handleIndex);
			return resultHandle;
		}
		else {
			m_handleEntries.emplace_back(static_cast<size_type>(m_gameObjects.size()), s_maxEntries, 0);
			
			InnerGameObjectHandle resultHandle(static_cast<size_type>(m_handleEntries.size() - 1), 0);
			gameObjectPointer->SetObjectHandle(resultHandle);
			m_gameObjects.emplace_back(std::move(gameObjectPointer));
			m_gameObjectHandleIndices.emplace_back(static_cast<size_type>(m_handleEntries.size() - 1));
			return resultHandle;
		}

	}
}

#endif