#include "stdafx.h"

bool Util::GetBinByFile(const char *cFile, std::string &sBin)
{
	FILE *fp = NULL;
	bool ret = false;
	int filelen = 0;

	if (cFile == NULL)
		goto end;

#ifdef _WIN32
	if (fopen_s(&fp, cFile, "rb") != 0)
		goto end;
#else
	if ((fp = fopen(cFile, "rb")) == NULL)
		goto end;
#endif


	if (fseek(fp, 0, SEEK_SET) != 0)
		goto end;

	if (fseek(fp, 0, SEEK_END) != 0)
		goto end;
	filelen = ftell(fp);
	sBin.resize(filelen);
	if (fseek(fp, 0, SEEK_SET) != 0)
		goto end;
	if (fread((char*)sBin.c_str(), 1, filelen, fp) < 1)
		goto end;

	ret = true;
end:
	if (fp) fclose(fp);
	return ret;
}

bool Util::GetBinFromHex(const char *cHex, std::string &sOutBin)
{
	size_t i = 0;
	unsigned int	*puTmpNum = NULL;
	size_t szLen = 0;
	char *cHexDup = NULL;
	bool ret = false;

	if (!cHex)
		goto end;

	cHexDup = (char*) malloc(strlen(cHex) + 1);
	memcpy(cHexDup, cHex, strlen(cHex));
	cHexDup[strlen(cHex)] = 0;

	if (strlen(cHexDup) % 2 != 0)
		goto end;

	szLen = strlen(cHexDup) / 2;

	sOutBin.resize(szLen);

	for (i = 0; i < strlen(cHexDup); i++)
	{
		if (!((cHexDup[i] >= '0' && cHexDup[i] <= '9') || (cHexDup[i] >= 'a' && cHexDup[i] <= 'f') || (cHexDup[i] >= 'A' && cHexDup[i] <= 'F')))
			goto end;
		cHexDup[i] = ::toupper(cHexDup[i]);
	}

	puTmpNum = (unsigned int*)malloc(szLen * sizeof(unsigned int));

	for (i = 0; i < szLen; i++)
	{
#ifdef _WIN32
		sscanf_s(cHexDup + i * 2, "%02X", puTmpNum + i);
#else
		sscanf(cHexDup + i * 2, "%02X", puTmpNum + i);
#endif
		((unsigned char*)sOutBin.c_str())[i] = (unsigned char)puTmpNum[i];
	}

	ret = true;
end:
	if (cHexDup) free(cHexDup);
	if (puTmpNum) free(puTmpNum);
	return ret;
}

bool Util::GetHexFromBin(const void *pData, size_t szLen, std::string &sOutStr)
{
	unsigned int i = 0;

	sOutStr.resize(szLen * 2);

	for (i = 0; i < szLen; i++)
	{
#ifdef _WIN32
		sprintf_s((char*)sOutStr.c_str() + 2 * i, sOutStr.size() - 2 * i + 1, "%02X", ((unsigned char*)pData)[i]);
#else
		sprintf((char*)sOutStr.c_str() + 2 * i, "%02X", ((unsigned char*)pData)[i]);
#endif
	}

	return true;
}

unsigned long long Util::GetRuntimeInMs()
{
#ifdef _WIN32
	return (unsigned long long)GetTickCount();
#else
	struct timespec clTime;
	clock_gettime(CLOCK_MONOTONIC_RAW, &clTime);
	return (((unsigned long long)clTime.tv_sec) * 1000) + (clTime.tv_nsec / 1000000);
#endif
}

#ifdef _WIN32

std::string Util::UnicodeToUTF8(const wchar_t* str)
{
	std::string	result;

	int n = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	if (n == 0)
		result.resize(0);
	else
		result.resize(n - 1);
	WideCharToMultiByte(CP_UTF8, 0, str, -1, (char*)result.c_str(), (int)result.length(), 0, 0);

	return result;
}

std::wstring Util::UTF8ToUnicode(const char* str)
{
	std::wstring	result;

	int n = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	if (n == 0)
		result.resize(0);
	else
		result.resize(n - 1);

	MultiByteToWideChar(CP_UTF8, 0, str, -1, (LPWSTR)result.c_str(), (int)result.length());

	return result;
}

#else

bool Util::ReadLinkAll(const char* cFile, std::string& sPath, bool* pbIsLink)
{
	bool ret = false;
	std::string sFile;
	std::string sTryPath;
	ssize_t szLength = 64;
	ssize_t rc = 0;

	sFile = cFile;

	while (1)
	{
		sTryPath.resize(szLength);
		rc = readlink(cFile, (char*)sTryPath.c_str(), (size_t)szLength);

		if (rc == -1)
		{
			if (errno == EINVAL || errno == ENOENT)
			{
				*pbIsLink = false;
				ret = true;
				goto end;
			}
			goto end;
		}

		*pbIsLink = true;

		if (rc < szLength - 1)
		{
			char* d;

			sTryPath.resize(rc);

			if (sTryPath.c_str()[0] != '/' && ((d = strrchr((char*)sFile.c_str(), '/'))))
			{
				/* Add path to relative link */
				*(d + 1) = 0;
				sPath = sFile.c_str();
				sPath += sTryPath;
			}
			else
			{
				sPath = sTryPath;
			}
			break;
		}

		/* Buffer too small, increase and retry */
		szLength *= 2;
	}

	ret = true;
end:
	return ret;
}

#endif
