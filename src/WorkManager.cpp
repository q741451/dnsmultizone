#include "stdafx.h"

WorkManager::ITEM_TYPE WorkManager::AllocWork()
{
	WorkManager::ITEM_TYPE spWork = std::make_shared<DNSQureyWork>();
	return spWork;
}

bool WorkManager::SaveWork(SOCKET_FD fd, ITEM_TYPE &wkWork)
{
	if (SaveItem(fd, wkWork) != true)
		return false;

	wkWork->SetInterface(this);

	return true;
}

void WorkManager::DNSQureyWorkClose(SOCKET_FD fdServer)
{
	WorkManager::ITEM_TYPE spDNSQureyWork;

	if (GetItem(fdServer, spDNSQureyWork) != true)
		return;

	spDNSQureyWork->Exit();
	DeleteItem(fdServer);
}

void WorkManager::ClearTimeout()
{
	std::map<SOCKET_FD, WorkManager::ITEM_TYPE>::iterator iterDNSQureyWork;
	unsigned long long llNow = Util::GetRuntimeInMs();

	SLOG_Info("ClearTimeout, all work count = %u", (unsigned int)m_mssKeyValuePairs.size());

	for (iterDNSQureyWork = m_mssKeyValuePairs.begin(); iterDNSQureyWork != m_mssKeyValuePairs.end(); )
	{
		if(llNow - iterDNSQureyWork->second->m_llLastTouch < 0 ||
			llNow - iterDNSQureyWork->second->m_llLastTouch > DEF_TIME_OUT)
		{
			iterDNSQureyWork->second->Exit();
			m_mssKeyValuePairs.erase(iterDNSQureyWork++);
		}
		else
			++iterDNSQureyWork;
	}

	SLOG_Info("ClearTimeout, Done!");
}

void WorkManager::ExitAndClear()
{
	std::map<SOCKET_FD, WorkManager::ITEM_TYPE>::iterator iterDNSQureyWork;

	for (iterDNSQureyWork = m_mssKeyValuePairs.begin(); iterDNSQureyWork != m_mssKeyValuePairs.end(); ++iterDNSQureyWork)
	{
		iterDNSQureyWork->second->Exit();
	}
	m_mssKeyValuePairs.clear();

	if (gServer.m_spConnectionManager->GetCount() != 0)
		SLOG_Error("ConnectionManager Count = %d", gServer.m_spConnectionManager->GetCount());
}
