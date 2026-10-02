#ifndef _DNS_CONNECT_H
#define _DNS_CONNECT_H


class InterfaceDNSConnect
{
public:
	virtual void DNSQueryDisconnect(unsigned int nIndex) = 0;
	virtual void DNSQueryResult(unsigned int nIndex, unsigned short nID, unsigned short uQType, unsigned short uFlag, bool bIsParseOK,
		std::list<unsigned int> &luIPs, std::list<IPv6Addr> &luIP6s, std::string &sDNSData) = 0;
};

// 一个 zone 的上游：connect 到上游地址的 UDP socket，一条查询一个报文
class DNSConnect
{
public:
	DNSConnect();
	~DNSConnect() {}

public:
	void SetInterface(InterfaceDNSConnect *ifInterface);
	bool Init(int fdSock, int fdEPoll);		// fdSock 为 -1：这个 zone 眼下没有可用的上游
	void Exit();

	int GetSockFd() { return m_fdSock; }

	void Read();
	void Write();
	void Disconnect();

	bool SendDNSQueryBuffer(std::string &sBuffer);

public:
	unsigned int m_nIndex;

private:
	static const unsigned int DEF_PKG_LEN = 0x1000;	// EDNS 应答常到 1232 字节

	InterfaceDNSConnect *m_ifInterface;
	int m_fdEPoll;
	int m_fdSock;
	std::list<std::string> m_lsWriteQueue;	// 发送缓冲满时暂存

	void UpdateEvents();
};

#endif
