#ifndef _SERVER_CONNECT_H
#define _SERVER_CONNECT_H

class InterfaceServerConnect
{
public:
	virtual void ServerNewWork(unsigned short nID, unsigned short uQType, std::string &sDNSData) = 0;
};

// 应答的去向：客户端地址，IPv6 查询还带着它发到的本机地址
struct ReplyAddr
{
	sockaddr_storage ssClient;
	bool bHasLocal;
	in6_pktinfo piLocal;
};

// 一个客户端：查询由共享监听 socket 收进来，应答也经它发回
class ServerConnect
{
public:
	ServerConnect();
	~ServerConnect() {}

public:
	void SetInterface(InterfaceServerConnect *ifInterface);
	void Init(const sockaddr_storage &addrClient);

	const sockaddr_storage &GetClientAddr() { return m_raReply.ssClient; }

	// 查询发到的本机地址（IPv6），应答从它发出
	void SetLocalAddr(const in6_pktinfo *pLocal);
	void OnRecvData(std::string &sDNSData);
	bool SendDNSResultBuffer(std::string &sBuffer);

private:
	InterfaceServerConnect *m_ifInterface;
	ReplyAddr m_raReply;
};

#endif
