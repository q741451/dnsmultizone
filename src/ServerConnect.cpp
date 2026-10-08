#include "stdafx.h"

ServerConnect::ServerConnect()
{
	m_ifInterface = NULL;
	memset(&m_raReply, 0, sizeof(m_raReply));
}

void ServerConnect::SetInterface(InterfaceServerConnect *ifInterface)
{
	m_ifInterface = ifInterface;
}

void ServerConnect::Init(const sockaddr_storage &addrClient)
{
	m_raReply.ssClient = addrClient;
}

void ServerConnect::SetLocalAddr(const in6_pktinfo *pLocal)
{
	m_raReply.bHasLocal = pLocal != NULL;
	if (pLocal)
		m_raReply.piLocal = *pLocal;
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
	return gServer.SendTo(m_raReply, sBuffer);
}
