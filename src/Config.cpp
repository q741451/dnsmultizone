#include "stdafx.h"

Config gConfig;

Config::Config()
{
	Reset();
}

bool Config::Init(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "b:l:c:")) != -1)
	{
		switch (ch)
		{
		case 'b':
			if (atoi(optarg) != 0)
				m_bIsBackMode = true;
			else
				m_bIsBackMode = false;
			break;
		case 'l':
			if (optarg)
				m_sFileLog = optarg;
			break;
		case 'c':
			if (optarg)
				m_sFileConfig = optarg;
			break;
		case '?':
			printf("Unknown option: %c\n", (char)optopt);
			break;
		}
	}

	return true;
}

void Config::Reset()
{
	m_bIsBackMode = false;
	m_sFileConfig = "DNSMZConfig.json";
}

static bool ZoneInfoCompare(const std::shared_ptr<ZoneInfo> &a, const std::shared_ptr<ZoneInfo> &b)
{
	return a->m_nPriority < b->m_nPriority;
}

bool Config::LoadConfigJson()
{
	bool ret = false;
	std::string sJson;
	cJSON *cjMonitor = NULL;
	cJSON *cjZones = NULL;
	cJSON *cjZone = NULL;


	if (Util::GetBinByFile(m_sFileConfig.c_str(), sJson) != true)
		goto end;

	if ((cjMonitor = cJSON_Parse(sJson.c_str())) == NULL)
		goto end;

	{
		cJSON *cjLocalIP = cJSON_GetObjectItemCaseSensitive(cjMonitor, "bindIP");
		cJSON *cjLocalPort = cJSON_GetObjectItemCaseSensitive(cjMonitor, "serverPort");

		if (!cJSON_IsString(cjLocalIP) || !cJSON_IsNumber(cjLocalPort))
			goto end;

		if (inet_pton(AF_INET, cjLocalIP->valuestring, &m_iaBindAddress) <= 0)
			goto end;

		m_uServerPort = cjLocalPort->valueint;
	}

	cjZones = cJSON_GetObjectItemCaseSensitive(cjMonitor, "zoneList");
	cJSON_ArrayForEach(cjZone, cjZones)
	{
		cJSON *cjName = cJSON_GetObjectItemCaseSensitive(cjZone, "name");
		cJSON *cjEnable = cJSON_GetObjectItemCaseSensitive(cjZone, "enable");
		cJSON *cjPriority = cJSON_GetObjectItemCaseSensitive(cjZone, "priority");
		cJSON* cjResolvFile = cJSON_GetObjectItemCaseSensitive(cjZone, "resolvFile");
		cJSON *cjDNSIP = cJSON_GetObjectItemCaseSensitive(cjZone, "dnsIP");
		cJSON *cjDNSPort = cJSON_GetObjectItemCaseSensitive(cjZone, "dnsPort");
		cJSON *cjIPs = cJSON_GetObjectItemCaseSensitive(cjZone, "ipList");
		cJSON *cjIP = NULL;

		std::shared_ptr<ZoneInfo> spZoneInfo;

		if (!cJSON_IsString(cjResolvFile) && !cJSON_IsString(cjDNSIP))
			goto end;

		if (!cJSON_IsString(cjName) || !cJSON_IsBool(cjEnable) || !cJSON_IsNumber(cjPriority) || !cJSON_IsNumber(cjDNSPort) || !cJSON_IsArray(cjIPs))
			goto end;

		if(!cJSON_IsTrue(cjEnable))
			continue;

		spZoneInfo = std::make_shared<ZoneInfo>();

		spZoneInfo->m_sName = cjName->valuestring;
		spZoneInfo->m_nPriority = cjPriority->valueint;
		
		if (cJSON_IsString(cjResolvFile))
		{
			spZoneInfo->m_rcfResolvConf.m_sResolvConfFile = cjResolvFile->valuestring;

			// 有文件，不读DNS
		}
		else
		{
			// 没文件，没dns直接给失败
			if (cJSON_IsString(cjDNSIP) && inet_pton(AF_INET, cjDNSIP->valuestring, &spZoneInfo->m_iaDNSAddr) <= 0)
				goto end;

			spZoneInfo->m_bIsDNSAddrOK = true;
		}

		spZoneInfo->m_nDNSPort = cjDNSPort->valueint;
		
		cJSON_ArrayForEach(cjIP, cjIPs)
		{
			cJSON *cjIPEnable = cJSON_GetObjectItemCaseSensitive(cjIP, "enable");
			cJSON* cjDeny = cJSON_GetObjectItemCaseSensitive(cjIP, "deny");
			cJSON *cjIPIsInverse = cJSON_GetObjectItemCaseSensitive(cjIP, "inverseIPList");
			cJSON *cjIPFile = cJSON_GetObjectItemCaseSensitive(cjIP, "file");

			std::shared_ptr<IPInfo> spIPInfo;

			if (!cJSON_IsBool(cjIPEnable) || !cJSON_IsBool(cjIPIsInverse)  || !cJSON_IsString(cjIPFile))
				goto end;

			if (!cJSON_IsTrue(cjIPEnable))
				continue;

			spIPInfo = std::make_shared<IPInfo>();

			spIPInfo->m_bIsInverseIPList = cJSON_IsTrue(cjIPIsInverse) ? true : false;
			if (cJSON_IsBool(cjDeny))
				spIPInfo->m_bIsDeny = cJSON_IsTrue(cjDeny) ? true : false;
			spIPInfo->m_sFileName = cjIPFile->valuestring;

			if (spIPInfo->LoadFile() != true)
			{
				printf("spIPInfo->LoadFile() failed\n");
				goto end;
			}

			spZoneInfo->m_siIPInfos.push_back(spIPInfo);
		}

		m_vsZoneInfos.push_back(spZoneInfo);
	}

	sort(m_vsZoneInfos.begin(), m_vsZoneInfos.end(), ZoneInfoCompare);

	ret = true;
end:
	if (cjMonitor) cJSON_Delete(cjMonitor);
	return ret;
}

bool Config::CheckIsMatch(unsigned int nIndex, unsigned int nIP)
{
	return m_vsZoneInfos[nIndex]->CheckIsMatch(nIP);
}

void Config::RefreshResolvConf()
{
	std::vector<std::shared_ptr<ZoneInfo>>::iterator iter;
	std::shared_ptr<ZoneInfo> spZoneInfo;
	struct stat statbuf;

	for (iter = m_vsZoneInfos.begin(); iter != m_vsZoneInfos.end(); ++iter)
	{
		spZoneInfo = (*iter);
		if (spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.size() == 0)
			continue;

		// 需要检验
		spZoneInfo->m_bIsDNSAddrOK = false;

		if (stat(spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str(), &statbuf) == -1)
		{
			SLOG_Error("Stat ResolvConfFile Error! Name = %s, File = %s", (*iter)->m_sName.c_str(), spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str());
			continue;
		}

		if (statbuf.st_mtime == spZoneInfo->m_rcfResolvConf.m_tResolvConfFileTime)
		{
			spZoneInfo->m_bIsDNSAddrOK = true;
			continue;
		}
		
		// 需要刷新
		memset(&spZoneInfo->m_iaDNSAddr, 0, sizeof(spZoneInfo->m_iaDNSAddr));
		SLOG_Info("Loading file = %s", spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str());

		spZoneInfo->m_rcfResolvConf.m_tResolvConfFileTime = statbuf.st_mtime;

		if (ReloadResolvConf(*spZoneInfo) != true)
		{
			SLOG_Error("Load ResolvConfFile Error! Name = %s, File = %s", (*iter)->m_sName.c_str(), spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str());
			continue;
		}

		spZoneInfo->m_bIsDNSAddrOK = true;
	}
}

bool Config::ReloadResolvConf(ZoneInfo &ziZoneInfo)
{
	bool ret = false;
	std::string sNameBuff;
	FILE* f = NULL;
	char* line = NULL;
	int gotone = 0;

	if ((f = fopen(ziZoneInfo.m_rcfResolvConf.m_sResolvConfFile.c_str(), "r")) == NULL)
	{
		SLOG_Error("FOpen ResolvConfFile Error! File = %s", ziZoneInfo.m_rcfResolvConf.m_sResolvConfFile.c_str());
		goto end;
	}

	sNameBuff.resize(1024);
	while ((line = fgets((char*)sNameBuff.c_str(), (int)sNameBuff.size(), f)))
	{
		char* token = strtok(line, " \t\n\r");

		if (!token)
			continue;

		if (strcmp(token, "nameserver") != 0)
			continue;

		if (!(token = strtok(NULL, " \t\n\r")))
			continue;

		if (inet_pton(AF_INET, token, &ziZoneInfo.m_iaDNSAddr) <= 0)
			continue;

		gotone = 1;
		break; // 只取第一个
	}

	if (gotone != 1)
		goto end;

	ret = true;
end:
	if (f != NULL)
		fclose(f);
	return ret;
}
