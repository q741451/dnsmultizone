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

bool DNSQureyWork::Init(const sockaddr_storage &addrClient, int fdEPoll)
{
	bool ret = false;
	int fd = -1;
	int i = 0;
	std::vector<std::shared_ptr<ZoneInfo>>::iterator iterZoneInfo;
	std::shared_ptr<DNSConnect> spDNSConnect;

	m_sKey = Util::AddrToString(addrClient);
	m_spServerConnect->Init(addrClient);

	SLOG_Debug("ADD client %s", m_sKey.c_str());

	for (iterZoneInfo = gConfig.m_vsZoneInfos.begin(), i = 0; iterZoneInfo != gConfig.m_vsZoneInfos.end(); ++iterZoneInfo, i++)
	{
		spDNSConnect = std::make_shared<DNSConnect>();
		if ((*iterZoneInfo)->m_bIsDNSAddrOK == false || (fd = ConnectToHost(*(*iterZoneInfo))) == -1)
		{
			// 挂了让他挂
			spDNSConnect->Init(-1, fdEPoll);
			if ((*iterZoneInfo)->m_bIsUpstreamDown == false)
			{
				(*iterZoneInfo)->m_bIsUpstreamDown = true;
				SLOG_Warn("zone %s: upstream %s unavailable", (*iterZoneInfo)->m_sName.c_str(),
					(*iterZoneInfo)->m_bIsDNSAddrOK ? Util::AddrToString((*iterZoneInfo)->m_ssDNSAddr).c_str() : "-");
			}
		}
		else
		{
			spDNSConnect->Init(fd, fdEPoll);
			if (gServer.m_spConnectionManager->SaveItem(fd, spDNSConnect) != true)
				goto end;
			if ((*iterZoneInfo)->m_bIsUpstreamDown)
			{
				(*iterZoneInfo)->m_bIsUpstreamDown = false;
				SLOG_Info("zone %s: upstream %s available", (*iterZoneInfo)->m_sName.c_str(),
					Util::AddrToString((*iterZoneInfo)->m_ssDNSAddr).c_str());
			}
			SLOG_Debug("ADD child fd %d zone %s upstream %s", fd, (*iterZoneInfo)->m_sName.c_str(),
				Util::AddrToString((*iterZoneInfo)->m_ssDNSAddr).c_str());
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
		if ((*iterDNSConnect)->GetSockFd() != -1)
		{
			SLOG_Debug("DEL child fd %d zone %s", (*iterDNSConnect)->GetSockFd(),
				gConfig.m_vsZoneInfos[(*iterDNSConnect)->m_nIndex]->m_sName.c_str());
			gServer.m_spConnectionManager->DeleteItem((*iterDNSConnect)->GetSockFd());
		}
		else
			SLOG_Debug("DEL child zone %s (no upstream)",
				gConfig.m_vsZoneInfos[(*iterDNSConnect)->m_nIndex]->m_sName.c_str());
			
		(*iterDNSConnect)->Exit();
	}

	SLOG_Debug("DEL client %s", m_sKey.c_str());
}

void DNSQureyWork::Clear()
{
	m_llLastTouch = 0;
	m_vsDNSConnects.clear();
	m_mwDNSQureyWorkItems.clear();
}

void DNSQureyWork::ServerNewWork(unsigned short nID, unsigned short uQType, std::string &sDNSData)
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
		spDNSQureyWorkItem->m_uQType = uQType;
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

void DNSQureyWork::DNSQueryResult(unsigned int nIndex, unsigned short nID, unsigned short uQType, unsigned short uFlag, bool bIsParseOK,
	std::list<unsigned int> &luIPs, std::list<IPv6Addr> &luIP6s, std::string &sDNSData)
{
	std::map<unsigned short, std::shared_ptr<DNSQureyWorkItem>>::iterator iterDNSQureyWorkItem;
	std::list<unsigned int>::iterator iterIP;
	std::list<IPv6Addr>::iterator iterIP6;
	unsigned int i = 0;
	unsigned int nMatchCount = 0;
	unsigned int nJudgedCount = 0;
	unsigned short uReqType = 0;
	bool bUseV4 = false;
	bool bUseV6 = false;

	iterDNSQureyWorkItem = m_mwDNSQureyWorkItems.find(nID);
	if (iterDNSQureyWorkItem == m_mwDNSQureyWorkItems.end())
		return;

	uReqType = iterDNSQureyWorkItem->second->m_uQType;

	// A 和 AAAA 是各走各的池，互不影响；SVCB/HTTPS 的 hint 两族一起看
	bUseV4 = (uReqType == Rfc1035::DEF_TYPE_A ||
		uReqType == Rfc1035::DEF_TYPE_SVCB || uReqType == Rfc1035::DEF_TYPE_HTTPS);
	bUseV6 = (uReqType == Rfc1035::DEF_TYPE_AAAA ||
		uReqType == Rfc1035::DEF_TYPE_SVCB || uReqType == Rfc1035::DEF_TYPE_HTTPS);

	if (sDNSData.size() == 0)
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_ERROR;
	else if (Rfc1035::IsIPBearingType(uReqType) == false)
	{
		// 该类型不携带任何 IP，判不出归属，也就没法说它不属于本域，
		// 按优先级取用即可
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_MATCH;
	}
	else if (bIsParseOK == false)
	{
		// 没看懂的应答不能升格成匹配，但仍可作兜底
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_NOT_MATCH;
	}
	else if (Rfc1035::IsIPBearingType(uQType) == false)
	{
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_NOT_MATCH;
	}
	else
	{
		// 本 zone 对某个族完全没有条目时不计入分子分母，它对该族没有立场
		if (bUseV4)
		{
			for (iterIP = luIPs.begin(); iterIP != luIPs.end(); ++iterIP)
			{
				EnumIPMatch eMatch = gConfig.CheckIsMatch(nIndex, (*iterIP));
				if (eMatch == ENUM_IP_NO_OPINION)
					continue;
				nJudgedCount++;
				if (eMatch == ENUM_IP_MATCH)
					nMatchCount++;
			}
		}

		if (bUseV6)
		{
			for (iterIP6 = luIP6s.begin(); iterIP6 != luIP6s.end(); ++iterIP6)
			{
				EnumIPMatch eMatch = gConfig.CheckIsMatch(nIndex, (*iterIP6).m_cAddr);
				if (eMatch == ENUM_IP_NO_OPINION)
					continue;
				nJudgedCount++;
				if (eMatch == ENUM_IP_MATCH)
					nMatchCount++;
			}
		}

		if (nJudgedCount == 0)
		{
			// 没有可判定的地址，要区分两种情况：
			// NXDOMAIN 在绝大多数环境里是合法的"这个名字不存在"，搜索域展开和
			// 拼错的域名每天都在产生，一律让位只会让它们白等所有 zone 回齐。
			// 只有确实用 NXDOMAIN 做列表屏蔽的上游才需要 denyNXDomain，此时它
			// 断言的"不存在"不可信，必须让位，否则被屏蔽的域名彻底解析不了。
			// 其余情况都判不出归属：真 NODATA 是这个名字确实没有该类型记录，
			// SERVFAIL 之类是上游自身故障，故障证明不了归属，都按优先级取用。
			if (gConfig.IsDenyNXDomain(nIndex) &&
				Rfc1035::GetRCode(uFlag) == Rfc1035::DEF_RCODE_NXDOMAIN)
				iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_NOT_MATCH;
			else
				iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_MATCH;
		}
		else if (nMatchCount != 0 && nJudgedCount / nMatchCount <= 2)
			iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_MATCH;
		else
			iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_eState = DNSQueryResultItem::ENUM_STATE_NOT_MATCH;

		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_luIPs = luIPs;
		iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[nIndex]->m_luIP6s = luIP6s;
	}

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
			// 可以了，通知客户
			m_spServerConnect->SendDNSResultBuffer(iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[i]->m_sDNSData);
			Finish(nID);
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
				m_spServerConnect->SendDNSResultBuffer(iterDNSQureyWorkItem->second->m_vsDNSQueryResultItems[i]->m_sDNSData);
				break;
			}
		}
		Finish(nID);
	}
}

// 这条查询答完了（或没有可给的应答）；事务里没有待答的查询就请管理器关掉它
void DNSQureyWork::Finish(unsigned short nID)
{
	m_mwDNSQureyWorkItems.erase(nID);
	if (m_mwDNSQureyWorkItems.empty() && m_ifInterface)
		m_ifInterface->DNSQureyWorkClose(m_sKey);
}

int DNSQureyWork::ConnectToHost(ZoneInfo &ziZoneInfo)
{
	bool ret = false;
	int fd = -1;

	// 上游是什么族就建什么族的 socket
	fd = socket(ziZoneInfo.m_ssDNSAddr.ss_family, SOCK_DGRAM, 0);

	if (fd == -1)
		goto end;

	if (connect(fd, (struct sockaddr*)&ziZoneInfo.m_ssDNSAddr, ziZoneInfo.GetDNSAddrLen()) != 0)
		goto end;

	ret = true;
end:
	if (ret == false)
	{
		if (fd != -1)
		{
			close(fd);
			fd = -1;
		}
	}
	return fd;
}

