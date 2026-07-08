#include "stdafx.h"

DNSQureyWork::DNSQureyWork()
{
	m_spServerConnect = std::make_shared<ServerConnect>();
	m_spServerConnect->SetInterface(this);
	Clear();
}

void DNSQureyWork::SetInterface(InterfaceDNSQureyWork *ifInterface)
{
	m_ifInterface = ifInterface;
}

bool DNSQureyWork::Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll, const sockaddr_in &addrAddrIn)
{
	bool ret = false;
	SOCKET_FD fd = (SOCKET_FD)-1;
	int i = 0;
	struct sockaddr_in saServer;
	std::vector<std::shared_ptr<ZoneInfo>>::iterator iterZoneInfo;
	std::shared_ptr<DNSConnect> spDNSConnect;
	std::shared_ptr<BaseConnect> spBaseConnect;

	if (m_spServerConnect->Init(fdSock, fdEPoll, addrAddrIn) != true)
		goto end;

	spBaseConnect = std::dynamic_pointer_cast<BaseConnect>(m_spServerConnect);
	if (gServer.m_spConnectionManager->SaveItem(fdSock, spBaseConnect) != true)
		goto end;

#ifdef WIN32
	if (gServer.m_spWin32ConnectionManager->SaveItem(m_spServerConnect->m_ulClientInfoWin32, fdSock) != true)
	{
		SLOG_Error("spWin32ConnectionManager SaveSession failed!\n");
		goto end;
	}
#endif

	SLOG_Info("ADD: %d", fdSock);

	for (iterZoneInfo = gConfig.m_vsZoneInfos.begin(), i = 0; iterZoneInfo != gConfig.m_vsZoneInfos.end(); ++iterZoneInfo, i++)
	{
		spDNSConnect = std::make_shared<DNSConnect>();
		memset(&saServer, 0, sizeof(saServer));
		saServer.sin_family = PF_INET;
		saServer.sin_addr = (*iterZoneInfo)->m_iaDNSAddr;
		saServer.sin_port = htons((*iterZoneInfo)->m_nDNSPort);
		if ((*iterZoneInfo)->m_bIsDNSAddrOK == false || (fd = ConnectToHost(saServer)) == (SOCKET_FD)-1)
		{
			// 挂了让他挂
			spDNSConnect->Init(-1, fdEPoll, saServer);
			SLOG_Info("ADD Error Child: %d", -1);
		}
		else
		{
			if (spDNSConnect->Init(fd, fdEPoll, saServer) != true)
				goto end;
			spBaseConnect = std::dynamic_pointer_cast<BaseConnect>(spDNSConnect);
			if (gServer.m_spConnectionManager->SaveItem(fd, spBaseConnect) != true)
				goto end;
			SLOG_Info("ADD Child: %d", fd);
		}
		spDNSConnect->m_nIndex = i;
		spDNSConnect->SetInterface(this);
		m_vsDNSConnects.push_back(spDNSConnect);
	}

	ret = true;
end:
	return ret;
}

void DNSQureyWork::Exit()
{
	std::vector<std::shared_ptr<DNSConnect>>::iterator iterDNSConnect;

	for (iterDNSConnect = m_vsDNSConnects.begin(); iterDNSConnect != m_vsDNSConnects.end(); ++iterDNSConnect)
	{
		if ((*iterDNSConnect)->GetSockFd() != (SOCKET_FD)-1)
		{
			SLOG_Info("DEL Child: %d", (*iterDNSConnect)->GetSockFd());
			gServer.m_spConnectionManager->DeleteItem((*iterDNSConnect)->GetSockFd());
		}
		else
			SLOG_Info("DEL Error Child: %d", (*iterDNSConnect)->GetSockFd());
			
		(*iterDNSConnect)->Exit();
	}

	SLOG_Info("DEL: %d", m_spServerConnect->GetSockFd());
	gServer.m_spConnectionManager->DeleteItem(m_spServerConnect->GetSockFd());
#ifdef WIN32
	gServer.m_spWin32ConnectionManager->DeleteItem(m_spServerConnect->m_ulClientInfoWin32);
#endif
	m_spServerConnect->Exit();
}

void DNSQureyWork::Clear()
{
	m_llLastTouch = 0;
	m_vsDNSConnects.clear();
	m_mwDNSQureyWorkItems.clear();
}

void DNSQureyWork::ServerDisconnect()
{
	if (m_ifInterface)
		m_ifInterface->DNSQureyWorkClose(m_spServerConnect->GetSockFd());
}

void DNSQureyWork::ServerNewWork(unsigned short nID, bool bIsARecord, std::string &sDNSData)
{
	std::map<unsigned short, std::shared_ptr<DNSQureyWorkItem>>::iterator iterDNSQureyWorkItem;
	std::vector<std::shared_ptr<DNSConnect>>::iterator iterDNSConnect;
	std::shared_ptr<DNSQureyWorkItem>	spDNSQureyWorkItem;
	std::shared_ptr<DNSQueryResultItem>		spDNSQueryResultItem;
	std::vector<std::shared_ptr<DNSQueryResultItem>>::iterator iterDNSQueryResultItem;
	int i = 0;

	m_llLastTouch = Util::GetRuntimeInMs();

	// 无ID的不处理
	if (sDNSData.size() < 2)
		return;

	iterDNSQureyWorkItem = m_mwDNSQureyWorkItems.find(nID);
	if (iterDNSQureyWorkItem != m_mwDNSQureyWorkItems.end())
	{
		// 未End的Session再发一遍
		spDNSQureyWorkItem = iterDNSQureyWorkItem->second;
		for (iterDNSQueryResultItem = spDNSQureyWorkItem->m_vsDNSQueryResultItems.begin(), i = 0;
			iterDNSQueryResultItem != spDNSQureyWorkItem->m_vsDNSQueryResultItems.end();
			++iterDNSQueryResultItem, i++)
		{
			if ((*iterDNSQueryResultItem)->m_eState == DNSQueryResultItem::ENUM_STATE_WAIT)
			{
				if (m_vsDNSConnects[i]->SendDNSQueryBuffer(sDNSData) != true)
					(*iterDNSQueryResultItem)->m_eState = DNSQueryResultItem::ENUM_STATE_ERROR;
			}
				
		}

		return;
	}
	else
	{
		// 创建一套，全部发一遍
		spDNSQureyWorkItem = std::make_shared<DNSQureyWorkItem>();
		spDNSQureyWorkItem->m_nID = nID;
		spDNSQureyWorkItem->m_bIsA = bIsARecord;
		for (iterDNSConnect = m_vsDNSConnects.begin(); iterDNSConnect != m_vsDNSConnects.end(); ++iterDNSConnect)
		{
			spDNSQueryResultItem = std::make_shared<DNSQueryResultItem>();
			spDNSQureyWorkItem->m_vsDNSQueryResultItems.push_back(spDNSQueryResultItem);
			if ((*iterDNSConnect)->SendDNSQueryBuffer(sDNSData) != true)
				spDNSQueryResultItem->m_eState = DNSQueryResultItem::ENUM_STATE_ERROR;
		}
		m_mwDNSQureyWorkItems[nID] = spDNSQureyWorkItem;

		return;
	}
}

void DNSQureyWork::DNSQueryDisconnect(unsigned int nIndex)
{
	// 该ID的所有正在等待的任务都挂了
	std::map<unsigned short, std::shared_ptr<DNSQureyWorkItem>>::iterator iterDNSQureyWorkItem;

	for (iterDNSQureyWorkItem = m_mwDNSQureyWorkItems.begin(); iterDNSQureyWorkItem != m_mwDNSQureyWorkItems.end(); ++iterDNSQureyWorkItem)
	{
		if (iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState == DNSQueryResultItem::ENUM_STATE_WAIT)
			iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_ERROR;
	}
}

void DNSQureyWork::DNSQueryResult(unsigned int nIndex, unsigned short nID, bool bIsA, std::list<unsigned int> &luIPs, std::string &sDNSData)
{
	std::map<unsigned short, std::shared_ptr<DNSQureyWorkItem>>::iterator iterDNSQureyWorkItem;
	std::list<unsigned int>::iterator iterIP;
	unsigned int i = 0;
	unsigned int nMatchCount = 0;
	
	iterDNSQureyWorkItem = m_mwDNSQureyWorkItems.find(nID);
	if (iterDNSQureyWorkItem == m_mwDNSQureyWorkItems.end())
		return;

	if (iterDNSQureyWorkItem->second->m_bIsDone)
		return;

	if (iterDNSQureyWorkItem->second->m_bIsA == false)
	{
		if (nIndex == 0)
		{
			// 非A记录，只处理首选DNS
			iterDNSQureyWorkItem->second->m_bIsDone = true;
			iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_sDNSData = sDNSData;
			m_spServerConnect->m_bDisconnectTag = true;
			m_spServerConnect->SendDNSResultBuffer(sDNSData);
		}
		return;
	}

	if(sDNSData.size() == 0)
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_ERROR;
	else if (bIsA == false)
	{
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_NOT_MATCH;
	}
	else if (luIPs.size() > 0)
	{
		for (iterIP = luIPs.begin(); iterIP != luIPs.end(); ++iterIP)
		{
			if (gConfig.CheckIsMatch(nIndex, (*iterIP)) == true)
				nMatchCount++;
		}
		if (nMatchCount != 0 && luIPs.size() / nMatchCount <= 2)
			iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_MATCH;
		else
			iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_NOT_MATCH;
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_luIPs = luIPs;
	}
	else if(luIPs.size() == 0)
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_NOT_MATCH;

	iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_sDNSData = sDNSData;
	
	for (i = iterDNSQureyWorkItem->second->m_nMaxDoneIndex; i < iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems.size(); i++)
	{
		if (iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[i]->m_eState == DNSQueryResultItem::ENUM_STATE_WAIT)
		{
			// 还未就绪，先不管
			iterDNSQureyWorkItem->second->m_nMaxDoneIndex = i;
			break;
		}

		if (iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[i]->m_eState == DNSQueryResultItem::ENUM_STATE_MATCH)
		{
			// 可以了，通知客户，清空这个事务
			iterDNSQureyWorkItem->second->m_bIsDone = true;
			m_spServerConnect->m_bDisconnectTag = true;
			m_spServerConnect->SendDNSResultBuffer(iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[i]->m_sDNSData);
			return;
		}
	}

	if (i == iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems.size())
	{
		// 都不符合，给优先级最高的合法的
		for (i = 0; i < iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems.size(); i++)
		{
			if (iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[i]->m_eState == DNSQueryResultItem::ENUM_STATE_NOT_MATCH)
			{
				m_spServerConnect->m_bDisconnectTag = true;
				m_spServerConnect->SendDNSResultBuffer(iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[i]->m_sDNSData);
				break;
			}
		}
		iterDNSQureyWorkItem->second->m_bIsDone = true;
	}
}

SOCKET_FD DNSQureyWork::ConnectToHost(struct sockaddr_in &saServer)
{
	bool ret = false;
	SOCKET_FD fd = (SOCKET_FD) -1;

	fd = socket(PF_INET, SOCK_DGRAM, 0);

	if (fd == (SOCKET_FD)-1)
		goto end;

	if (connect(fd, (struct sockaddr*)&saServer, sizeof(saServer)) != 0)
		goto end;

	ret = true;
end:
	if (ret == false)
	{
		if (fd != (SOCKET_FD)-1)
		{
			SOCKET_CLOSE(fd);
			fd = (SOCKET_FD)-1;
		}
	}
	return fd;
}

