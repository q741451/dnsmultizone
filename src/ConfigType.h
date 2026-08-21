#ifndef _CONFIG_TYPE_H
#define _CONFIG_TYPE_H

// 一条 ipList 对某个地址的判定结果。
// NO_OPINION 表示这份表里没有该地址族的任何条目，也就谈不上"属于"或"不属于"，
// 由上层按优先级处理，不能当成否定。
enum EnumIPMatch
{
	ENUM_IP_NO_OPINION,
	ENUM_IP_MATCH,
	ENUM_IP_NOT_MATCH,
};

// IPv6 地址按网络字节序原样存放，字典序即数值序，排序/比较/二分全部用 memcmp
class IPv6Addr
{
public:
	IPv6Addr() { memset(m_cAddr, 0, sizeof(m_cAddr)); }
	unsigned char m_cAddr[16];
};

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

class IPItemInfo6
{
public:
	IPItemInfo6() { m_nPrefix = 128; }
	bool SetFromString(const char *cIP);
	int MatchCompare(const unsigned char *cIP)
	{
		unsigned char cMaskIP[16];

		ApplyMask(cIP, m_nPrefix, cMaskIP);
		return memcmp(cMaskIP, m_iaAddr.m_cAddr, sizeof(cMaskIP));
	}
	static void ApplyMask(const unsigned char *cIP, unsigned int nPrefix, unsigned char *cOut)
	{
		unsigned int i = 0;

		for (i = 0; i < 16; i++)
		{
			unsigned int nBitLeft = (nPrefix > i * 8) ? (nPrefix - i * 8) : 0;

			if (nBitLeft >= 8)
				cOut[i] = cIP[i];
			else if (nBitLeft == 0)
				cOut[i] = 0;
			else
				cOut[i] = (unsigned char)(cIP[i] & (0xFF << (8 - nBitLeft)));
		}
	}
	IPv6Addr m_iaAddr;
	unsigned int m_nPrefix;
};

class IPInfo
{
public:
	IPInfo() {
		m_bIsDeny = false;
		m_bIsInverseIPList = false;
	}
	bool LoadFile();
	EnumIPMatch CheckIsMatch(unsigned int nIP);
	EnumIPMatch CheckIsMatch(const unsigned char *cIP);
	bool m_bIsDeny;
	bool m_bIsInverseIPList;
	std::string m_sFileName;
	std::vector<std::shared_ptr<IPItemInfo>> m_siIPItems;
	std::vector<std::shared_ptr<IPItemInfo6>> m_siIPItems6;
};

class ResolvConf
{
public:
	ResolvConf() {
		m_tResolvConfFileTime = 0;
	}
	std::string m_sResolvConfFile;
	time_t m_tResolvConfFileTime;
};

class ZoneInfo
{
public:
	ZoneInfo() {
		m_nPriority = 0;
		memset(&m_ssDNSAddr, 0, sizeof(m_ssDNSAddr));
		m_bIsDNSAddrOK = false;
		m_nDNSPort = 0;
		m_bIsDenyNXDomain = false;
	}
	std::string m_sName;
	unsigned int m_nPriority;
	ResolvConf m_rcfResolvConf;
	bool m_bIsDNSAddrOK;
	// 这个上游是否用 NXDOMAIN 屏蔽域名。默认 false，即把 NXDOMAIN 当作
	// 正常的"名字不存在"，按优先级取用；置 true 才视为否定并让位
	bool m_bIsDenyNXDomain;
	struct sockaddr_storage m_ssDNSAddr;	// 上游地址，v4/v6 都放这里
	unsigned short m_nDNSPort;
	bool SetDNSAddrFromString(const char *cAddr);
	void SetDNSPort(unsigned short nPort);
	socklen_t GetDNSAddrLen() const;
	EnumIPMatch CheckIsMatch(unsigned int nIP);
	EnumIPMatch CheckIsMatch(const unsigned char *cIP);
	std::vector<std::shared_ptr<IPInfo>> m_siIPInfos;
};

#endif


