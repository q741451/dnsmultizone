#ifndef _DNS_CONNECT_H
#define _DNS_CONNECT_H


class InterfaceDNSConnect
{
public:
	virtual void DNSQueryDisconnect(unsigned int nIndex) = 0;
	virtual void DNSQueryResult(unsigned int nIndex, unsigned short nID, unsigned short uQType, bool bIsParseOK,
		std::list<unsigned int> &luIPs, std::list<IPv6Addr> &luIP6s, std::string &sDNSData) = 0;
};

class DNSConnect : public BaseConnect
{
public:
	DNSConnect();
	~DNSConnect() {}

public:
	void SetInterface(InterfaceDNSConnect *ifInterface);
	virtual bool Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll);
	virtual void Exit();
	virtual void Clear();

	virtual bool Read();
	virtual bool Write();
	virtual void Disconnect();

	bool SendDNSQueryBuffer(std::string &sBuffer);

public:
	unsigned int m_nIndex;

private:
	static const unsigned int DEF_CLIENT_PKG_LEN = 0x400;

	InterfaceDNSConnect *m_ifInterface;

	bool DoNextEPollEvent();

	void OnRecvData();
};

#endif
