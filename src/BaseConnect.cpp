#include "stdafx.h"

BaseConnect::BaseConnect()
{
	Clear();
}

bool BaseConnect::Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll)
{
	bool ret = false;
	int iError = 0;
	socklen_t slLen = sizeof(iError);

	m_fdSock = fdSock;
	m_fdEPoll = fdEPoll;

	if (getsockopt(m_fdSock, SOL_SOCKET, SO_ERROR, (char*)&iError, &slLen) != 0)
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
	m_sReadBuff.clear();
	m_sWriteBuff.clear();
	m_lsReadQueue.clear();
	m_lsWriteQueue.clear();
}

SOCKET_FD BaseConnect::GetSockFd()
{
	return m_fdSock;
}

