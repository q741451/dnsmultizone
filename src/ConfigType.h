#ifndef _CONFIG_TYPE_H
#define _CONFIG_TYPE_H

class IPItemInfo
{
public:
	IPItemInfo() {
		m_nIP = 0;
		m_nMask = UINT32_MAX;
	}
	bool SetFromString(const char *cIP);
	int MatchCompare(unsigned int nIP)
	{
		unsigned int nMaskIP = nIP & m_nMask;
		if (nMaskIP == m_nIP)
			return 0;

		if (nMaskIP > m_nIP)
			return 1;

		return -1;
	}
	unsigned int m_nIP;
	unsigned int m_nMask;
};

class IPInfo
{
public:
	IPInfo() {
		m_bIsDeny = false;
		m_bIsInverseIPList = false;
	}
	bool LoadFile();
	bool CheckIsMatch(unsigned int nIP);
	bool m_bIsDeny;
	bool m_bIsInverseIPList;
	std::string m_sFileName;
	std::vector<std::shared_ptr<IPItemInfo>> m_siIPItems;
};

class ResolvConf
{
public:
	ResolvConf() {
		m_tResolvConfFileTime = 0;
		memset(&m_iaDNSAddr, 0, sizeof(m_iaDNSAddr));
	}
	std::string m_sResolvConfFile;
	time_t m_tResolvConfFileTime;
	struct in_addr m_iaDNSAddr;
};

class ZoneInfo
{
public:
	ZoneInfo() {
		m_nPriority = 0;
		memset(&m_iaDNSAddr, 0, sizeof(m_iaDNSAddr));
		m_bIsDNSAddrOK = false;
		m_nDNSPort = 0;
	}
	std::string m_sName;
	unsigned int m_nPriority;
	ResolvConf m_rcfResolvConf;
	bool m_bIsDNSAddrOK;
	struct in_addr m_iaDNSAddr;
	unsigned short m_nDNSPort;
	bool CheckIsMatch(unsigned int nIP);
	std::vector<std::shared_ptr<IPInfo>> m_siIPInfos;
};

#endif


