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
	unsigned int nLineNo = 0;

	bool ret = false;

	if ((fpFile = fopen(m_sFileName.c_str(), "rb")) == NULL)
		return false;

	if (fseek(fpFile, 0, SEEK_SET) != 0)
		goto end;

	while ((cLine = fgets(cLineBuf, nLen, fpFile)))
	{
		IPItemInfo iiItem;
		IPItemInfo6 iiItem6;
		char *sp_pos;

		nLineNo++;
		sp_pos = strchr(cLine, '\r');
		if (sp_pos) *sp_pos = 0;
		sp_pos = strchr(cLine, '\n');
		if (sp_pos) *sp_pos = 0;

		// 不是地址的行（注释等）照旧跳过；另一族的地址说明文件放错了
		if (iiItem.SetFromString(cLine))
		{
			if (m_nFamily != 4)
			{
				SLOG_Error("ipList %s is IPv6, line %u is IPv4: %s", m_sFileName.c_str(), nLineNo, cLine);
				goto end;
			}
			m_siIPItems.push_back(std::make_shared<IPItemInfo>(iiItem));
		}
		else if (iiItem6.SetFromString(cLine))
		{
			if (m_nFamily != 6)
			{
				SLOG_Error("ipList %s is IPv4, line %u is IPv6: %s", m_sFileName.c_str(), nLineNo, cLine);
				goto end;
			}
			m_siIPItems6.push_back(std::make_shared<IPItemInfo6>(iiItem6));
		}
	}
	sort(m_siIPItems.begin(), m_siIPItems.end(), CompareIPItemInfo);
	sort(m_siIPItems6.begin(), m_siIPItems6.end(), CompareIPItemInfo6);

	SLOG_Info("Load ipList %s, IPv%d = %u", m_sFileName.c_str(), m_nFamily,
		(unsigned int)(m_nFamily == 4 ? m_siIPItems.size() : m_siIPItems6.size()));

	ret = true;
end:
	if (fpFile != NULL) fclose(fpFile);
	return ret;
}

bool IPInfo::CheckIsMatch(unsigned int nIP)
{
	int nBegin = 0, nEnd = (int)m_siIPItems.size() - 1;
	int nMid = 0, iCmpResult = 0;
	bool ret = false;

	if (m_siIPItems.size() == 0)
		return m_bIsInverseIPList;

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

	return m_bIsInverseIPList ? !ret : ret;
}

bool IPInfo::CheckIsMatch(const unsigned char *cIP)
{
	int nBegin = 0, nEnd = (int)m_siIPItems6.size() - 1;
	int nMid = 0, iCmpResult = 0;
	bool ret = false;

	if (m_siIPItems6.size() == 0)
		return m_bIsInverseIPList;

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

	return m_bIsInverseIPList ? !ret : ret;
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

// 按 ipList 顺序只问这一族的表，第一条命中的说了算（deny 命中即否定）；
// 都没命中就不属于本 zone
bool ZoneInfo::CheckIsMatch(unsigned int nIP)
{
	std::vector<std::shared_ptr<IPInfo>>::iterator iterIPInfo;

	for (iterIPInfo = m_siIPInfos.begin(); iterIPInfo != m_siIPInfos.end(); ++iterIPInfo)
	{
		if ((*iterIPInfo)->m_nFamily == 4 && (*iterIPInfo)->CheckIsMatch(nIP))
			return !(*iterIPInfo)->m_bIsDeny;
	}
	return false;
}

bool ZoneInfo::CheckIsMatch(const unsigned char *cIP)
{
	std::vector<std::shared_ptr<IPInfo>>::iterator iterIPInfo;

	for (iterIPInfo = m_siIPInfos.begin(); iterIPInfo != m_siIPInfos.end(); ++iterIPInfo)
	{
		if ((*iterIPInfo)->m_nFamily == 6 && (*iterIPInfo)->CheckIsMatch(cIP))
			return !(*iterIPInfo)->m_bIsDeny;
	}
	return false;
}
