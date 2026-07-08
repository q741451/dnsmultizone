#ifndef _SERVER_CONNECT_H
#define _SERVER_CONNECT_H

class InterfaceServerConnect
{
public:
	virtual void ServerDisconnect() = 0;
	virtual void ServerNewWork(unsigned short nID, bool bIsARecord, std::string &sDNSData) = 0;
};

class ServerConnect : public BaseConnect
{
public:
	ServerConnect();
	~ServerConnect() {}

public:
	void SetInterface(InterfaceServerConnect *ifInterface);
	bool PrepareRecvByRecvFrom(SOCKET_FD fdSock, sockaddr *addrFrom, socklen_t *pLenAddrFrom);
	virtual bool Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll, const sockaddr_in &addrAddrIn);
	virtual void Exit();
	virtual void Clear();

	virtual bool Read();
	virtual bool Write();
	virtual void Disconnect();

	bool SendDNSResultBuffer(std::string &sBuffer);

	void OnRecvDataFirst();

	bool		m_bDisconnectTag;

#ifdef WIN32
	unsigned long long m_ulClientInfoWin32;
#endif

private:
	static const unsigned int DEF_CLIENT_PKG_LEN = 0x400;

	InterfaceServerConnect *m_ifInterface;

	bool DoNextEPollEvent();

	void OnRecvData();		// 上来就要调用一次
};

#endif
