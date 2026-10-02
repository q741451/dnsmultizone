#include "stdafx.h"

ServerConnect::ServerConnect()
{
	m_ifInterface = NULL;
	memset(&m_addrClient, 0, sizeof(m_addrClient));
}

void ServerConnect::SetInterface(InterfaceServerConnect *ifInterface)
{
	m_ifInterface = ifInterface;
}

void ServerConnect::Init(const sockaddr_storage &addrClient)
{
	m_addrClient = addrClient;
}

void ServerConnect::OnRecvData(std::string &sDNSData)
{
	unsigned short uFlag = 0;
	std::string sName;
	unsigned short uID = 0;
	unsigned short uQType = 0;

	Rfc1035::ParseRequestA(sDNSData, &uID, &uFlag, sName, &uQType);

	if (m_ifInterface)
		m_ifInterface->ServerNewWork(uID, uQType, sDNSData);
}

bool ServerConnect::SendDNSResultBuffer(std::string &sBuffer)
{
	return gServer.SendTo(m_addrClient, sBuffer);
}
