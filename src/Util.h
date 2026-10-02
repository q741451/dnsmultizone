#ifndef _UTIL_H
#define _UTIL_H

class Util
{
public:
	static bool GetBinByFile(const char *cFile, std::string &sBin);

	static unsigned long long GetRuntimeInMs();

	// 格式为 "1.2.3.4:53" / "[::1]:53"，未设置时返回 "-"
	static std::string AddrToString(const sockaddr_storage &ssAddr);

	static bool ReadLinkAll(const char *cFile, std::string& sPath, bool* pbIsLink);
};

#endif
