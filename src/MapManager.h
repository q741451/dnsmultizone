#ifndef _MAP_MANAGER_H
#define _MAP_MANAGER_H

template <class KeyType, class ValueType>
class MapManager
{
public:
	typedef ValueType ITEM_TYPE;

	MapManager() {}
	~MapManager() {}

	bool SaveItem(KeyType kyKey, ITEM_TYPE &itItem)
	{
		typename std::map<KeyType, ValueType>::iterator iterForMap;

		iterForMap = m_mssKeyValuePairs.find(kyKey);
		if (iterForMap != m_mssKeyValuePairs.end())
			return false;
		m_mssKeyValuePairs[kyKey] = itItem;
		return true;
	}
	bool GetItem(KeyType kyKey, ITEM_TYPE &itItem)
	{
		typename std::map<KeyType, ValueType>::iterator iterForMap;

		iterForMap = m_mssKeyValuePairs.find(kyKey);
		if (iterForMap == m_mssKeyValuePairs.end())
			return false;
		itItem = iterForMap->second;
		return true;
	}
	bool GetFirstItem(ITEM_TYPE &itItem)
	{
		if (m_mssKeyValuePairs.begin() == m_mssKeyValuePairs.end())
			return false;

		itItem = m_mssKeyValuePairs.begin()->second;
		return true;
	}
	bool DeleteItem(KeyType kyKey)
	{
		typename std::map<KeyType, ValueType>::iterator iterForMap;

		iterForMap = m_mssKeyValuePairs.find(kyKey);
		if (iterForMap == m_mssKeyValuePairs.end())
			return false;

		m_mssKeyValuePairs.erase(iterForMap);

		return true;
	}

	unsigned int GetCount()
	{
		return (unsigned int)m_mssKeyValuePairs.size();
	}

protected:
	std::map<KeyType, ValueType>		m_mssKeyValuePairs;
};


#endif
