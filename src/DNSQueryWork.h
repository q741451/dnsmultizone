#ifndef _DNS_QUERY_WORK_H
#define _DNS_QUERY_WORK_H

class InterfaceDNSQureyWork
{
public:
	virtual void DNSQureyWorkClose(const std::string &sKey) = 0;
};

class DNSQureyWork : public InterfaceDNSConnect, public InterfaceServerConnect
{
public:
	DNSQureyWork();
	~DNSQureyWork() {}

public:
	void SetInterface(InterfaceDNSQureyWork *ifInterface);
	bool Init(const sockaddr_storage &addrClient, int fdEPoll);
	void Exit();
	void Clear();

	bool IsIdle() { return m_mwDNSQureyWorkItems.empty(); }

	// Server
	virtual void ServerNewWork(unsigned short nID, unsigned short uQType, std::string &sDNSData);

	// DNS
	virtual void DNSQueryDisconnect(unsigned int nIndex);
	virtual void DNSQueryResult(unsigned int nIndex, unsigned short nID, unsigned short uQType, unsigned short uFlag, bool bIsParseOK,
		std::list<unsigned int> &luIPs, std::list<IPv6Addr> &luIP6s, std::string &sDNSData);

	unsigned long long							m_llLastTouch;
	std::shared_ptr<ServerConnect>				m_spServerConnect;
	std::vector<std::shared_ptr<DNSConnect>>					m_vsDNSConnects;
private:
	InterfaceDNSQureyWork	*m_ifInterface;

	std::string								m_sKey;
	std::map<unsigned short, std::shared_ptr<DNSQureyWorkItem>>	m_mwDNSQureyWorkItems;

	int ConnectToHost(ZoneInfo &ziZoneInfo);
	void Finish(unsigned short nID);
	void SendResult(unsigned int nIndex, unsigned short uQType, std::string &sDNSData);
};



#endif
