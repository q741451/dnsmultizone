#ifndef _DNS_QUERY_WORK_H
#define _DNS_QUERY_WORK_H

class InterfaceDNSQureyWork
{
public:
	virtual void DNSQureyWorkClose(SOCKET_FD fdServer) = 0;
};

class DNSQureyWork : public InterfaceDNSConnect, public InterfaceServerConnect
{
public:
	DNSQureyWork();
	~DNSQureyWork() {}

public:
	void SetInterface(InterfaceDNSQureyWork *ifInterface);
	bool Init(SOCKET_FD fdSock, EPOLL_FD fdEPoll, const sockaddr_in &addrAddrIn);
	void Exit();
	void Clear();

	// Server
	virtual void ServerDisconnect();
	virtual void ServerNewWork(unsigned short nID, bool bIsARecord, std::string &sDNSData);

	// DNS
	virtual void DNSQueryDisconnect(unsigned int nIndex);
	virtual void DNSQueryResult(unsigned int nIndex, unsigned short nID, bool bIsA, std::list<unsigned int> &luIPs, std::string &sDNSData);

	unsigned long long							m_llLastTouch;
	std::shared_ptr<ServerConnect>				m_spServerConnect;
	std::vector<std::shared_ptr<DNSConnect>>					m_vsDNSConnects;
private:
	InterfaceDNSQureyWork	*m_ifInterface;

	std::map<unsigned short, std::shared_ptr<DNSQureyWorkItem>>	m_mwDNSQureyWorkItems;

	SOCKET_FD ConnectToHost(struct sockaddr_in &saServer);
};



#endif
