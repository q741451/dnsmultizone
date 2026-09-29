#ifndef _SERVER_CONN_H
#define _SERVER_CONN_H


class BaseConnect
{
public:
	BaseConnect();
	virtual ~BaseConnect() {}

public:
	// 地址只在 ServerConnect 侧有意义，这里不需要
	virtual bool Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll);
	virtual void Exit();
	virtual void Clear();

	virtual bool Read() = 0;
	virtual bool Write() = 0;
	virtual void Disconnect() = 0;

	SOCKET_FD GetSockFd();
	const sockaddr_storage &GetClientAddr() { return m_addrClient; }

	void SetPreReadBuff(std::string &sPreReadBuff);

protected:
	EPOLL_FD m_fdEPoll;
	SOCKET_FD m_fdSock;
	sockaddr_storage m_addrClient;

	std::string m_sReadBuff;
	unsigned int m_nReadOffset;
	std::string m_sWriteBuff;
	unsigned int m_nWriteOffset;
	std::list<std::string> m_lsReadQueue;
	std::list<std::string> m_lsWriteQueue;
	std::string m_sPreReadBuff;
};

#endif
