#include "stdafx.h"

bool IPItemInfo::SetFromString(const char *cIP)
{
	std::string sPureIP;
	std::string sMask;
	struct in_addr iaAddr;
	const char *cPos = strchr(cIP, '/');

	if (cPos)
	{
		sPureIP.resize(cPos - cIP);
		memcpy((char*)sPureIP.c_str(), cIP, cPos - cIP);
		sMask = cPos + 1;
		int nPrefix = atoi(sMask.c_str());
		if (nPrefix < 0 || nPrefix > 32)
			return false;
		if (nPrefix == 0)
			m_nMask = 0;
		else
			m_nMask = ~((unsigned int)(((unsigned long long)1) << (32 - nPrefix)) - 1);
	}
	else
	{
		sPureIP = cIP;
		m_nMask = UINT32_MAX;
	}

	if (inet_pton(AF_INET, sPureIP.c_str(), &iaAddr) <= 0)
		return false;

	m_nIP = ntohl(iaAddr.s_addr) & m_nMask;
	return true;
}

bool IPItemInfo6::SetFromString(const char *cIP)
{
	std::string sPureIP;
	std::string sMask;
	struct in6_addr iaAddr;
	unsigned char cMasked[16];
	const char *cPos = strchr(cIP, '/');

	if (cPos)
	{
		sPureIP.assign(cIP, cPos - cIP);
		sMask = cPos + 1;
		int nPrefix = atoi(sMask.c_str());
		if (nPrefix < 0 || nPrefix > 128)
			return false;
		m_nPrefix = (unsigned int)nPrefix;
	}
	else
	{
		sPureIP = cIP;
		m_nPrefix = 128;
	}

	if (inet_pton(AF_INET6, sPureIP.c_str(), &iaAddr) <= 0)
		return false;

	ApplyMask((const unsigned char*)&iaAddr, m_nPrefix, cMasked);
	memcpy(m_iaAddr.m_cAddr, cMasked, sizeof(cMasked));
	return true;
}

static bool CompareIPItemInfo(const std::shared_ptr<IPItemInfo> &a, const std::shared_ptr<IPItemInfo> &b)
{
	return a->m_nIP < b->m_nIP;
}

static bool CompareIPItemInfo6(const std::shared_ptr<IPItemInfo6> &a, const std::shared_ptr<IPItemInfo6> &b)
{
	return memcmp(a->m_iaAddr.m_cAddr, b->m_iaAddr.m_cAddr, sizeof(a->m_iaAddr.m_cAddr)) < 0;
}

bool IPInfo::LoadFile()
{
	FILE *fpFile = NULL;
	char cLineBuf[128];
	char *cLine;
	unsigned int nLen = sizeof(cLineBuf);

	bool ret = false;

	if ((fpFile = fopen(m_sFileName.c_str(), "rb")) == NULL)
		return false;

	if (fseek(fpFile, 0, SEEK_SET) != 0)
		goto end;

	while ((cLine = fgets(cLineBuf, nLen, fpFile)))
	{
		char *sp_pos;
		sp_pos = strchr(cLine, '\r');
		if (sp_pos) *sp_pos = 0;
		sp_pos = strchr(cLine, '\n');
		if (sp_pos) *sp_pos = 0;

		if (strlen(cLine) > 0)
		{
			// 同一个文件里可以混放 v4/v6，按冒号分桶
			if (strchr(cLine, ':') != NULL)
			{
				std::shared_ptr<IPItemInfo6> spIPItem6 = std::make_shared<IPItemInfo6>();
				if (spIPItem6->SetFromString(cLine) != true)
					continue;
				m_siIPItems6.push_back(spIPItem6);
			}
			else
			{
				std::shared_ptr<IPItemInfo> spIPItem = std::make_shared<IPItemInfo>();
				if(spIPItem->SetFromString(cLine) != true)
					continue;
				m_siIPItems.push_back(spIPItem);
			}
		}

	}
	sort(m_siIPItems.begin(), m_siIPItems.end(), CompareIPItemInfo);
	sort(m_siIPItems6.begin(), m_siIPItems6.end(), CompareIPItemInfo6);

	SLOG_Info("Load ipList %s, IPv4 = %u, IPv6 = %u",
		m_sFileName.c_str(), (unsigned int)m_siIPItems.size(), (unsigned int)m_siIPItems6.size());

	ret = true;
end:
	if (fpFile != NULL) fclose(fpFile);
	return ret;
}

EnumIPMatch IPInfo::CheckIsMatch(unsigned int nIP)
{
	int nBegin = 0, nEnd = (int)m_siIPItems.size() - 1;
	int nMid = 0, iCmpResult = 0;
	bool ret = false;

	// 这份表对 IPv4 没有任何条目，也就没有立场判定
	if (m_siIPItems.size() == 0)
		return ENUM_IP_NO_OPINION;

	do
	{
		nMid = (nBegin + nEnd) / 2;

		iCmpResult = m_siIPItems[nMid]->MatchCompare(nIP);
		if (iCmpResult < 0) {
			if (nEnd != nMid)
				nEnd = nMid;
			else
			{
				if (m_siIPItems[nBegin]->MatchCompare(nIP) == 0)
					ret = true;
				break;
			}
		}
		else if (iCmpResult > 0)
		{
			if (nBegin != nMid)
				nBegin = nMid;
			else
			{
				if (m_siIPItems[nEnd]->MatchCompare(nIP) == 0)
					ret = true;
				break;
			};
		}
		else {
			ret = true;
			break;
		}
	}
	while (nBegin != nEnd);

	if (m_bIsInverseIPList)
		ret = !ret;

	return ret ? ENUM_IP_MATCH : ENUM_IP_NOT_MATCH;
}

EnumIPMatch IPInfo::CheckIsMatch(const unsigned char *cIP)
{
	int nBegin = 0, nEnd = (int)m_siIPItems6.size() - 1;
	int nMid = 0, iCmpResult = 0;
	bool ret = false;

	// 这份表对 IPv6 没有任何条目，也就没有立场判定
	if (m_siIPItems6.size() == 0)
		return ENUM_IP_NO_OPINION;

	do
	{
		nMid = (nBegin + nEnd) / 2;

		iCmpResult = m_siIPItems6[nMid]->MatchCompare(cIP);
		if (iCmpResult < 0) {
			if (nEnd != nMid)
				nEnd = nMid;
			else
			{
				if (m_siIPItems6[nBegin]->MatchCompare(cIP) == 0)
					ret = true;
				break;
			}
		}
		else if (iCmpResult > 0)
		{
			if (nBegin != nMid)
				nBegin = nMid;
			else
			{
				if (m_siIPItems6[nEnd]->MatchCompare(cIP) == 0)
					ret = true;
				break;
			};
		}
		else {
			ret = true;
			break;
		}
	}
	while (nBegin != nEnd);

	if (m_bIsInverseIPList)
		ret = !ret;

	return ret ? ENUM_IP_MATCH : ENUM_IP_NOT_MATCH;
}

bool ZoneInfo::SetDNSAddrFromString(const char *cAddr)
{
	struct sockaddr_in *psa4 = (struct sockaddr_in*)&m_ssDNSAddr;
	struct sockaddr_in6 *psa6 = (struct sockaddr_in6*)&m_ssDNSAddr;

	memset(&m_ssDNSAddr, 0, sizeof(m_ssDNSAddr));

	if (strchr(cAddr, ':') != NULL)
	{
		if (inet_pton(AF_INET6, cAddr, &psa6->sin6_addr) <= 0)
			return false;
		psa6->sin6_family = AF_INET6;
		return true;
	}

	if (inet_pton(AF_INET, cAddr, &psa4->sin_addr) <= 0)
		return false;
	psa4->sin_family = AF_INET;
	return true;
}

void ZoneInfo::SetDNSPort(unsigned short nPort)
{
	if (m_ssDNSAddr.ss_family == AF_INET6)
		((struct sockaddr_in6*)&m_ssDNSAddr)->sin6_port = htons(nPort);
	else
		((struct sockaddr_in*)&m_ssDNSAddr)->sin_port = htons(nPort);
}

socklen_t ZoneInfo::GetDNSAddrLen() const
{
	if (m_ssDNSAddr.ss_family == AF_INET6)
		return sizeof(struct sockaddr_in6);
	return sizeof(struct sockaddr_in);
}

// 按 ipList 顺序逐条问，第一条有立场的说了算；全都没立场就是 NO_OPINION
EnumIPMatch ZoneInfo::CheckIsMatch(unsigned int nIP)
{
	std::vector<std::shared_ptr<IPInfo>>::iterator iterIPInfo;
	bool bHasOpinion = false;

	for (iterIPInfo = m_siIPInfos.begin(); iterIPInfo != m_siIPInfos.end(); ++iterIPInfo)
	{
		EnumIPMatch eMatch = (*iterIPInfo)->CheckIsMatch(nIP);

		if (eMatch == ENUM_IP_NO_OPINION)
			continue;

		bHasOpinion = true;

		if (eMatch == ENUM_IP_MATCH)
		{
			if ((*iterIPInfo)->m_bIsDeny)
				return ENUM_IP_NOT_MATCH;
			else
				return ENUM_IP_MATCH;
		}
	}

	return bHasOpinion ? ENUM_IP_NOT_MATCH : ENUM_IP_NO_OPINION;
}

EnumIPMatch ZoneInfo::CheckIsMatch(const unsigned char *cIP)
{
	std::vector<std::shared_ptr<IPInfo>>::iterator iterIPInfo;
	bool bHasOpinion = false;

	for (iterIPInfo = m_siIPInfos.begin(); iterIPInfo != m_siIPInfos.end(); ++iterIPInfo)
	{
		EnumIPMatch eMatch = (*iterIPInfo)->CheckIsMatch(cIP);

		if (eMatch == ENUM_IP_NO_OPINION)
			continue;

		bHasOpinion = true;

		if (eMatch == ENUM_IP_MATCH)
		{
			if ((*iterIPInfo)->m_bIsDeny)
				return ENUM_IP_NOT_MATCH;
			else
				return ENUM_IP_MATCH;
		}
	}

	return bHasOpinion ? ENUM_IP_NOT_MATCH : ENUM_IP_NO_OPINION;
}
