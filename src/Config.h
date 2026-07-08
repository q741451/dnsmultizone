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
	bool CheckIsMatch(unsigned int nIndex, unsigned int nIP);

	void RefreshResolvConf();

	// 传入配置
	std::string m_sFileConfig;			// 详细配置
	bool		m_bIsBackMode;			// 是否后台模式
	std::string m_sFileLog;				// 后台Log位置

	// Json
	struct in_addr m_iaBindAddress;
	unsigned short m_uServerPort;
	std::vector<std::shared_ptr<ZoneInfo>> m_vsZoneInfos;

private:
	bool ReloadResolvConf(ZoneInfo& ziZoneInfo);
};

extern Config gConfig;

#endif


