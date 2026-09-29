#ifndef _UTIL_H
#define _UTIL_H

class Util
{
public:
	static bool GetBinByFile(const char *cFile, std::string &sBin);

	static bool GetBinFromHex(const char *cHex, std::string &sOutBin);

	static bool GetHexFromBin(const void *pData, size_t szLen, std::string &sOutStr);

	static unsigned long long GetRuntimeInMs();

	// 格式为 "1.2.3.4:53" / "[::1]:53"，未设置时返回 "-"
	static std::string AddrToString(const sockaddr_storage &ssAddr);

#ifdef _WIN32
	static std::string UnicodeToUTF8(const wchar_t* str);

	static std::wstring UTF8ToUnicode(const char* str);
#else
	static bool ReadLinkAll(const char *cFile, std::string& sPath, bool* pbIsLink);
#endif
};

#endif
