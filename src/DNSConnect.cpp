#include "stdafx.h"

DNSConnect::DNSConnect()
{
	m_ifInterface = NULL;
	Clear();
}

void DNSConnect::SetInterface(InterfaceDNSConnect *ifInterface)
{
	m_ifInterface = ifInterface;
}

bool DNSConnect::Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll)
{
	if (BaseConnect::Init(fdSock, fdEPoll) != true)
		return false;

	// 初始化时候只有读
	m_sReadBuff.resize(DEF_CLIENT_PKG_LEN);
	m_nReadOffset = 0;
	addfd(m_fdEPoll, fdSock, EPOLLIN, true);

	return true;
}

void DNSConnect::Exit()
{
	// 程序退出
	BaseConnect::Exit();
}

void DNSConnect::Clear()
{
	BaseConnect::Clear();
	m_nIndex = 0;
}

bool DNSConnect::Read()
{
	int iLen = 0;

	if (m_sPreReadBuff.size() > 0)
	{
		iLen = (int)((m_sReadBuff.size() - m_nReadOffset) > m_sPreReadBuff.size() ? m_sPreReadBuff.size() : (m_sReadBuff.size() - m_nReadOffset));
		memcpy((char*)m_sReadBuff.c_str() + m_nReadOffset, m_sPreReadBuff.c_str(), iLen);
		m_sPreReadBuff.clear();
	}
	else
	{
		if ((iLen = recv(m_fdSock, (char*)m_sReadBuff.c_str() + m_nReadOffset, (int)m_sReadBuff.size() - m_nReadOffset, 0)) < 0 && (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN))
			return false;
	}

	if (iLen > 0)
	{
		m_nReadOffset += iLen;
	}

	// 错误
	if (m_nReadOffset > m_sReadBuff.size())
		return false;

	// 这里不继续读
	m_sReadBuff.resize(m_nReadOffset);

	// 解析
	OnRecvData();

	// 继续读
	m_sReadBuff.resize(DEF_CLIENT_PKG_LEN);
	m_nReadOffset = 0;
	DoNextEPollEvent();

	return true;
}

bool DNSConnect::Write()
{
	int iLen = 0;

	if (m_lsWriteQueue.size() == 0 && m_nWriteOffset == m_sWriteBuff.size())
	{
		// 无任务
		return true;
	}

	if (m_nWriteOffset == m_sWriteBuff.size() && m_lsWriteQueue.size() != 0)
	{
		// 没准备好就取出一个出来
		m_sWriteBuff = *m_lsWriteQueue.begin();
		m_lsWriteQueue.pop_front();
		m_nWriteOffset = 0;
	}

	if ((iLen = send(m_fdSock, (char*)m_sWriteBuff.c_str() + m_nWriteOffset, (int)m_sWriteBuff.size() - m_nWriteOffset, 0)) < 0 && (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN && errno != EINPROGRESS))
		return false;

	if (iLen > 0)
	{
		m_nWriteOffset += iLen;
	}

	// 错误
	if (m_nWriteOffset > m_sWriteBuff.size())
		return false;

	// 继续发送
	if (m_nWriteOffset < m_sWriteBuff.size())
	{
		if (DoNextEPollEvent() != true)
			return false;
		return true;
	}

	if (DoNextEPollEvent() != true) // 是否继续发
		return false;

	return true;
}

void DNSConnect::Disconnect()
{
	if (m_ifInterface)
		m_ifInterface->DNSQueryDisconnect(m_nIndex);
}

bool DNSConnect::SendDNSQueryBuffer(std::string &sBuffer)
{
	m_lsWriteQueue.push_back(sBuffer);

	if (DoNextEPollEvent() != true)
		return false;

	return true;
}

bool DNSConnect::DoNextEPollEvent()
{
	bool ret = false;

	if (m_nWriteOffset > m_sWriteBuff.size())
		goto end;

	if (m_sWriteBuff.size() != m_nWriteOffset || m_lsWriteQueue.size() != 0)
	{
		if (modfd(m_fdEPoll, m_fdSock, EPOLLIN | EPOLLOUT) != 0)
			goto end;
	}
	else
	{
		if (modfd(m_fdEPoll, m_fdSock, EPOLLIN) != 0)
			goto end;
	}

	ret = true;
end:
	return ret;
}

void DNSConnect::OnRecvData()
{
	unsigned short uFlag = 0;
	std::string sName;
	std::string sDNSData;
	unsigned short uID = 0;
	unsigned short uQType = 0;
	std::list<unsigned int> luIPs;
	std::list<IPv6Addr> luIP6s;
	bool bIsParseOK = false;

	sDNSData = m_sReadBuff;

	if (Rfc1035::ParseResponseA(sDNSData, &uID, &uFlag, sName, &uQType, luIPs, luIP6s) != true)
		goto end;

	bIsParseOK = true;
end:
	if (m_ifInterface)
		m_ifInterface->DNSQueryResult(m_nIndex, uID, uQType, uFlag, bIsParseOK, luIPs, luIP6s, sDNSData);
}

