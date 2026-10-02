#ifndef _SERVER_CONNECT_H
#define _SERVER_CONNECT_H

class InterfaceServerConnect
{
public:
	virtual void ServerNewWork(unsigned short nID, unsigned short uQType, std::string &sDNSData) = 0;
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

	const sockaddr_storage &GetClientAddr() { return m_addrClient; }

	void OnRecvData(std::string &sDNSData);
	bool SendDNSResultBuffer(std::string &sBuffer);

private:
	InterfaceServerConnect *m_ifInterface;
	sockaddr_storage m_addrClient;
};

#endif
