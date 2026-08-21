#ifndef _CONFIG_H
#define _CONFIG_H

class Config
{
public:
	Config();
	~Config() {};

	bool Init(int argc, char *argv[]);
	void Reset();
	bool LoadConfigJson();
	EnumIPMatch CheckIsMatch(unsigned int nIndex, unsigned int nIP);
	EnumIPMatch CheckIsMatch(unsigned int nIndex, const unsigned char *cIP);

	void RefreshResolvConf();

	// 传入配置
	std::string m_sFileConfig;			// 详细配置
	bool		m_bIsBackMode;			// 是否后台模式
	std::string m_sFileLog;				// 后台Log位置

	// Json
	struct sockaddr_storage m_ssBindAddress;	// 监听地址，v4/v6 都放这里
	socklen_t m_slBindAddressLen;
	unsigned short m_uServerPort;
	std::vector<std::shared_ptr<ZoneInfo>> m_vsZoneInfos;

private:
	bool ReloadResolvConf(ZoneInfo& ziZoneInfo);
};

extern Config gConfig;

#endif


