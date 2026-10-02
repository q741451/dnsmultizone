#include "stdafx.h"

DNSConnect::DNSConnect()
{
	m_ifInterface = NULL;
	m_fdEPoll = -1;
	m_fdSock = -1;
	m_nIndex = 0;
}

void DNSConnect::SetInterface(InterfaceDNSConnect *ifInterface)
{
	m_ifInterface = ifInterface;
}

bool DNSConnect::Init(int fdSock, int fdEPoll)
{
	m_fdSock = fdSock;
	m_fdEPoll = fdEPoll;

	if (m_fdSock == -1)
		return false;

	addfd(m_fdEPoll, m_fdSock, EPOLLIN, false);
	return true;
}

void DNSConnect::Exit()
{
	if (m_fdSock != -1)
	{
		removefd(m_fdEPoll, m_fdSock);
		close(m_fdSock);
		m_fdSock = -1;
	}
}

void DNSConnect::Read()
{
	std::string sDNSData;
	std::string sName;
	std::list<unsigned int> luIPs;
	std::list<IPv6Addr> luIP6s;
	unsigned short uFlag = 0;
	unsigned short uID = 0;
	unsigned short uQType = 0;
	bool bIsParseOK = false;
	int iLen = 0;

	sDNSData.resize(DEF_PKG_LEN);
	if ((iLen = recv(m_fdSock, (char*)sDNSData.c_str(), sDNSData.size(), 0)) <= 0)
		return;
	sDNSData.resize(iLen);

	bIsParseOK = Rfc1035::ParseResponseA(sDNSData, &uID, &uFlag, sName, &uQType, luIPs, luIP6s);

	if (m_ifInterface)
		m_ifInterface->DNSQueryResult(m_nIndex, uID, uQType, uFlag, bIsParseOK, luIPs, luIP6s, sDNSData);
}

void DNSConnect::Write()
{
	while (m_lsWriteQueue.size() > 0)
	{
		if (send(m_fdSock, m_lsWriteQueue.front().c_str(), m_lsWriteQueue.front().size(), 0) < 0 &&
			(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
			break;
		// 发出去了，或者是发不出去的错误：都不再留着
		m_lsWriteQueue.pop_front();
	}

	UpdateEvents();
}

// 上游报错（如 ICMP 不可达）：不再监听这个 socket，否则水平触发下错误会每轮都报
void DNSConnect::Disconnect()
{
	removefd(m_fdEPoll, m_fdSock);
	if (m_ifInterface)
		m_ifInterface->DNSQueryDisconnect(m_nIndex);
}

bool DNSConnect::SendDNSQueryBuffer(std::string &sBuffer)
{
	if (m_fdSock == -1)
		return false;

	if (m_lsWriteQueue.size() == 0)
	{
		if (send(m_fdSock, sBuffer.c_str(), sBuffer.size(), 0) >= 0)
			return true;
		if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN)
			return false;
	}

	m_lsWriteQueue.push_back(sBuffer);
	UpdateEvents();
	return true;
}

void DNSConnect::UpdateEvents()
{
	epoll_event event;

	event.data.fd = m_fdSock;
	event.events = EPOLLIN | EPOLLRDHUP;
	if (m_lsWriteQueue.size() > 0)
		event.events |= EPOLLOUT;
	epoll_ctl(m_fdEPoll, EPOLL_CTL_MOD, m_fdSock, &event);
}
