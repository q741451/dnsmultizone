#include "stdafx.h"

Config gConfig;

Config::Config()
{
	Reset();
}

bool Config::Init(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "b:l:c:p:")) != -1)
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
		case 'p':
		{
			char *pEnd = NULL;
			long lPort = strtol(optarg, &pEnd, 10);

			if (*optarg == '\0' || *pEnd != '\0' || lPort <= 0 || lPort > 65535)
			{
				printf("Invalid port: %s\n", optarg);
				return false;
			}
			m_nPortOverride = (int)lPort;
			break;
		}
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
	m_nPortOverride = 0;
	memset(&m_ssBindAddress, 0, sizeof(m_ssBindAddress));
	m_slBindAddressLen = 0;
}

// 读配置文件，再用命令行覆盖全局项
bool Config::LoadConfig()
{
	struct sockaddr_in *psa4 = (struct sockaddr_in*)&m_ssBindAddress;
	struct sockaddr_in6 *psa6 = (struct sockaddr_in6*)&m_ssBindAddress;

	if (LoadConfigJson() != true)
		return false;

	if (m_nPortOverride != 0)
	{
		m_uServerPort = (unsigned short)m_nPortOverride;
		if (m_ssBindAddress.ss_family == AF_INET6)
			psa6->sin6_port = htons(m_uServerPort);
		else
			psa4->sin_port = htons(m_uServerPort);
	}

	return true;
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

		m_uServerPort = cjLocalPort->valueint;

		// bindIP 填 v6 地址(含 ::)时监听 socket 走双栈，同时收 IPv4
		memset(&m_ssBindAddress, 0, sizeof(m_ssBindAddress));
		if (strchr(cjLocalIP->valuestring, ':') != NULL)
		{
			struct sockaddr_in6 *psa6 = (struct sockaddr_in6*)&m_ssBindAddress;

			if (inet_pton(AF_INET6, cjLocalIP->valuestring, &psa6->sin6_addr) <= 0)
				goto end;
			psa6->sin6_family = AF_INET6;
			psa6->sin6_port = htons(m_uServerPort);
			m_slBindAddressLen = sizeof(struct sockaddr_in6);
		}
		else
		{
			struct sockaddr_in *psa4 = (struct sockaddr_in*)&m_ssBindAddress;

			if (inet_pton(AF_INET, cjLocalIP->valuestring, &psa4->sin_addr) <= 0)
				goto end;
			psa4->sin_family = AF_INET;
			psa4->sin_port = htons(m_uServerPort);
			m_slBindAddressLen = sizeof(struct sockaddr_in);
		}
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
		cJSON *cjDenyNX = cJSON_GetObjectItemCaseSensitive(cjZone, "denyNXDomain");
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
			if (cJSON_IsString(cjDNSIP) && spZoneInfo->SetDNSAddrFromString(cjDNSIP->valuestring) != true)
				goto end;

			spZoneInfo->m_bIsDNSAddrOK = true;
		}

		spZoneInfo->m_nDNSPort = cjDNSPort->valueint;
		spZoneInfo->SetDNSPort(spZoneInfo->m_nDNSPort);

		// 可选项，缺省为 false
		if (cJSON_IsBool(cjDenyNX))
			spZoneInfo->m_bIsDenyNXDomain = cJSON_IsTrue(cjDenyNX) ? true : false;
		
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

EnumIPMatch Config::CheckIsMatch(unsigned int nIndex, unsigned int nIP)
{
	return m_vsZoneInfos[nIndex]->CheckIsMatch(nIP);
}

EnumIPMatch Config::CheckIsMatch(unsigned int nIndex, const unsigned char *cIP)
{
	return m_vsZoneInfos[nIndex]->CheckIsMatch(cIP);
}

bool Config::IsDenyNXDomain(unsigned int nIndex)
{
	return m_vsZoneInfos[nIndex]->m_bIsDenyNXDomain;
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
		memset(&spZoneInfo->m_ssDNSAddr, 0, sizeof(spZoneInfo->m_ssDNSAddr));
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

		// v4 v6 都接受，仍然只取第一条能解析的
		if (ziZoneInfo.SetDNSAddrFromString(token) != true)
			continue;

		gotone = 1;
		break; // 只取第一个
	}

	if (gotone != 1)
		goto end;

	ziZoneInfo.SetDNSPort(ziZoneInfo.m_nDNSPort);

	ret = true;
end:
	if (f != NULL)
		fclose(f);
	return ret;
}
