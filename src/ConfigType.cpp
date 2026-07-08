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
		m_nMask = ~((unsigned int)(((unsigned long long)1) << (32 - atoi(sMask.c_str()))) - 1);
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

static bool CompareIPItemInfo(const std::shared_ptr<IPItemInfo> &a, const std::shared_ptr<IPItemInfo> &b)
{
	return a->m_nIP < b->m_nIP;
}

bool IPInfo::LoadFile()
{
	FILE *fpFile = NULL;
	char cLineBuf[32];
	char *cLine;
	unsigned int nLen = sizeof(cLineBuf);

	bool ret = false;

#ifdef _WIN32
	if (fopen_s(&fpFile, m_sFileName.c_str(), "rb") != 0)
		return false;
#else
	if ((fpFile = fopen(m_sFileName.c_str(), "rb")) == NULL)
		return false;
#endif

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
			std::shared_ptr<IPItemInfo> spIPItem = std::make_shared<IPItemInfo>();
			if(spIPItem->SetFromString(cLine) != true)
				continue;
			m_siIPItems.push_back(spIPItem);
		}
		
	}
	sort(m_siIPItems.begin(), m_siIPItems.end(), CompareIPItemInfo);

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
		goto end;

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

end:
	if (m_bIsInverseIPList)
		return !ret;
	else
		return ret;
}


bool ZoneInfo::CheckIsMatch(unsigned int nIP)
{
	std::vector<std::shared_ptr<IPInfo>>::iterator iterIPInfo;

	for (iterIPInfo = m_siIPInfos.begin(); iterIPInfo != m_siIPInfos.end(); ++iterIPInfo)
	{
		if ((*iterIPInfo)->CheckIsMatch(nIP) == true)
		{
			if ((*iterIPInfo)->m_bIsDeny)
				return false;
			else
				return true;
		}	
	}

	return false;
}
