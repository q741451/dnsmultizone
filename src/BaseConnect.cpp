#include "stdafx.h"

BaseConnect::BaseConnect()
{
	Clear();
}

bool BaseConnect::Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll, const sockaddr_in &addrAddrIn)
{
	bool ret = false;
	int iError = 0;
	int iReuse = 1;
	socklen_t slLen = sizeof(iError);

	m_fdSock = fdSock;
	m_addrClient = addrAddrIn;
	m_fdEPoll = fdEPoll;

	if (getsockopt(m_fdSock, SOL_SOCKET, SO_ERROR, (char*)&iError, &slLen) != 0)
		goto end;
	if (setsockopt(m_fdSock, SOL_SOCKET, SO_REUSEADDR, (char*)&iReuse, sizeof(iReuse)) != 0)
		goto end;

	ret = true;
end:
	return ret;
}

void BaseConnect::Exit()
{
	if (m_fdSock != (SOCKET_FD)-1)
	{
		removefd(m_fdEPoll, m_fdSock);
		SOCKET_CLOSE(m_fdSock);
		m_fdSock = (SOCKET_FD)-1;
	}
}

void BaseConnect::Clear()
{
	m_fdEPoll = (EPOLL_FD)-1;
	m_fdSock = (SOCKET_FD)-1;
	m_nReadOffset = 0;
	m_nWriteOffset = 0;
	memset(&m_addrClient, 0, sizeof(m_addrClient));
	m_sReadBuff.clear();
	m_sWriteBuff.clear();
	m_lsReadQueue.clear();
	m_lsWriteQueue.clear();
}

SOCKET_FD BaseConnect::GetSockFd()
{
	return m_fdSock;
}

void BaseConnect::SetPreReadBuff(std::string &sPreReadBuff)
{
	m_sPreReadBuff = sPreReadBuff;
}
