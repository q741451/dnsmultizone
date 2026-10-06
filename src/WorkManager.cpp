#include "stdafx.h"

WorkManager::ITEM_TYPE WorkManager::AllocWork()
{
	WorkManager::ITEM_TYPE spWork = std::make_shared<DNSQureyWork>();
	return spWork;
}

bool WorkManager::SaveWork(const std::string &sKey, ITEM_TYPE &wkWork)
{
	if (SaveItem(sKey, wkWork) != true)
		return false;

	wkWork->SetInterface(this);

	return true;
}

void WorkManager::DNSQureyWorkClose(const std::string &sKey)
{
	m_ssIdle.insert(sKey);
}

// 记下之后同一轮里又来了新查询的事务，留着
void WorkManager::CloseIdle()
{
	std::set<std::string>::iterator iterKey;
	WorkManager::ITEM_TYPE spDNSQureyWork;

	for (iterKey = m_ssIdle.begin(); iterKey != m_ssIdle.end(); ++iterKey)
	{
		if (GetItem(*iterKey, spDNSQureyWork) != true || spDNSQureyWork->IsIdle() != true)
			continue;

		spDNSQureyWork->Exit();
		DeleteItem(*iterKey);
	}
	m_ssIdle.clear();
}

void WorkManager::ClearTimeout()
{
	std::map<std::string, WorkManager::ITEM_TYPE>::iterator iterDNSQureyWork;
	unsigned long long llNow = Util::GetRuntimeInMs();

	size_t szBefore = m_mssKeyValuePairs.size();

	for (iterDNSQureyWork = m_mssKeyValuePairs.begin(); iterDNSQureyWork != m_mssKeyValuePairs.end(); )
	{
		if (llNow - iterDNSQureyWork->second->m_llLastTouch > DEF_TIME_OUT)
		{
			iterDNSQureyWork->second->Exit();
			m_mssKeyValuePairs.erase(iterDNSQureyWork++);
		}
		else
			++iterDNSQureyWork;
	}

	SLOG_Debug("ClearTimeout %u -> %u sessions", (unsigned int)szBefore, (unsigned int)m_mssKeyValuePairs.size());
}

void WorkManager::ExitAndClear()
{
	std::map<std::string, WorkManager::ITEM_TYPE>::iterator iterDNSQureyWork;

	for (iterDNSQureyWork = m_mssKeyValuePairs.begin(); iterDNSQureyWork != m_mssKeyValuePairs.end(); ++iterDNSQureyWork)
	{
		iterDNSQureyWork->second->Exit();
	}
	m_mssKeyValuePairs.clear();

	if (gServer.m_spConnectionManager->GetCount() != 0)
		SLOG_Error("ConnectionManager Count = %d", gServer.m_spConnectionManager->GetCount());
}
