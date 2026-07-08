#ifndef _UTIL_H
#define _UTIL_H

class Util
{
public:
	static bool GetBinByFile(const char *cFile, std::string &sBin);

	static bool GetBinFromHex(const char *cHex, std::string &sOutBin);

	static bool GetHexFromBin(const void *pData, size_t szLen, std::string &sOutStr);

	static unsigned long long GetRuntimeInMs();

#ifdef _WIN32
	static std::string UnicodeToUTF8(const wchar_t* str);

	static std::wstring UTF8ToUnicode(const char* str);
#else
	static bool ReadLinkAll(const char *cFile, std::string& sPath, bool* pbIsLink);
#endif
};

#endif
