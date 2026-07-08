#include "stdafx.h"

ServerConnect::ServerConnect()
{
	m_ifInterface = NULL;
	Clear();
}

void ServerConnect::SetInterface(InterfaceServerConnect *ifInterface)
{
	m_ifInterface = ifInterface;
}

bool ServerConnect::PrepareRecvByRecvFrom(SOCKET_FD fdSock, sockaddr *addrFrom, socklen_t *pLenAddrFrom)
{
	int iLen = 0;

	m_sReadBuff.resize(DEF_CLIENT_PKG_LEN);
	m_nReadOffset = 0;

	if (m_sPreReadBuff.size() > 0)
	{
		iLen = (int)((m_sReadBuff.size() - m_nReadOffset) > m_sPreReadBuff.size() ? m_sPreReadBuff.size() : (m_sReadBuff.size() - m_nReadOffset));
		memcpy((char*)m_sReadBuff.c_str() + m_nReadOffset, m_sPreReadBuff.c_str(), iLen);
		m_sPreReadBuff.clear();
	}
	else
	{
		if ((iLen = recvfrom(fdSock, (char*)m_sReadBuff.c_str() + m_nReadOffset, (int)m_sReadBuff.size() - m_nReadOffset, 0, addrFrom, pLenAddrFrom)) < 0)
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

	// 暂不解析

	return true;
}

bool ServerConnect::Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll, const sockaddr_in &addrAddrIn)
{
	bool ret = false;
	int iError = 0;
	socklen_t slLen = sizeof(iError);

	m_fdSock = fdSock;
	m_addrClient = addrAddrIn;
	m_fdEPoll = fdEPoll;

	if (getsockopt(m_fdSock, SOL_SOCKET, SO_ERROR, (char*)&iError, &slLen) != 0)
		goto end;

	// 读取的数据由PrepareRecvByRecvFrom立即准备并处理

	ret = true;
end:
	return ret;
}

void ServerConnect::Exit()
{
	// 程序退出
	BaseConnect::Exit();
}

void ServerConnect::Clear()
{
	m_bDisconnectTag = false;
	BaseConnect::Clear();
}


bool ServerConnect::Read()
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

bool ServerConnect::Write()
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

void ServerConnect::Disconnect()
{
	if (m_ifInterface)
		m_ifInterface->ServerDisconnect();
}

bool ServerConnect::SendDNSResultBuffer(std::string &sBuffer)
{
	m_lsWriteQueue.push_back(sBuffer);

	if (DoNextEPollEvent() != true)
		return false;

	return true;
}

void ServerConnect::OnRecvDataFirst()
{
	// 解析
	OnRecvData();

	// 开始读
	m_sReadBuff.resize(DEF_CLIENT_PKG_LEN);
	m_nReadOffset = 0;
	addfd(m_fdEPoll, m_fdSock, EPOLLIN, true);
}

bool ServerConnect::DoNextEPollEvent()
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
		if (m_bDisconnectTag == true)
		{
			if (m_ifInterface)
				m_ifInterface->ServerDisconnect();
		}
	}

	ret = true;
end:
	return ret;
}

void ServerConnect::OnRecvData()
{
	unsigned short uFlag = 0;
	std::string sName;
	std::string sDNSData;
	unsigned short uID = 0;
	bool bIsA = false;
	std::list<unsigned int> luIPs;
	// bool ret = false;

	sDNSData = m_sReadBuff;

	if (Rfc1035::ParseRequestA(sDNSData, &uID, &uFlag, sName, &bIsA) != true)
		goto end;

	// ret = true;
end:
	if (m_ifInterface)
		m_ifInterface->ServerNewWork(uID, bIsA, sDNSData);
}


