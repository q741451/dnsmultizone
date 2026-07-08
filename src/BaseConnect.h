#ifndef _SERVER_CONN_H
#define _SERVER_CONN_H


class BaseConnect
{
public:
	BaseConnect();
	virtual ~BaseConnect() {}

public:
	virtual bool Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll, const sockaddr_in &addrAddrIn);
	virtual void Exit();
	virtual void Clear();

	virtual bool Read() = 0;
	virtual bool Write() = 0;
	virtual void Disconnect() = 0;

	SOCKET_FD GetSockFd();

	void SetPreReadBuff(std::string &sPreReadBuff);

protected:
	EPOLL_FD m_fdEPoll;
	SOCKET_FD m_fdSock;
	sockaddr_in m_addrClient;

	std::string m_sReadBuff;
	unsigned int m_nReadOffset;
	std::string m_sWriteBuff;
	unsigned int m_nWriteOffset;
	std::list<std::string> m_lsReadQueue;
	std::list<std::string> m_lsWriteQueue;
	std::string m_sPreReadBuff;
};

#endif
