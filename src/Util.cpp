#include "stdafx.h"

bool Util::GetBinByFile(const char *cFile, std::string &sBin)
{
	FILE *fp = NULL;
	bool ret = false;
	int filelen = 0;

	if (cFile == NULL)
		goto end;

	if ((fp = fopen(cFile, "rb")) == NULL)
		goto end;


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

unsigned long long Util::GetRuntimeInMs()
{
	struct timespec clTime;
	clock_gettime(CLOCK_MONOTONIC_RAW, &clTime);
	return (((unsigned long long)clTime.tv_sec) * 1000) + (clTime.tv_nsec / 1000000);
}

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

std::string Util::AddrToString(const sockaddr_storage &ssAddr)
{
	char cIP[INET6_ADDRSTRLEN] = { 0 };
	char cOut[INET6_ADDRSTRLEN + 16] = { 0 };

	if (ssAddr.ss_family == AF_INET)
	{
		const struct sockaddr_in *psa4 = (const struct sockaddr_in*)&ssAddr;

		inet_ntop(AF_INET, &psa4->sin_addr, cIP, sizeof(cIP));
		snprintf(cOut, sizeof(cOut), "%s:%u", cIP, ntohs(psa4->sin_port));
	}
	else if (ssAddr.ss_family == AF_INET6)
	{
		const struct sockaddr_in6 *psa6 = (const struct sockaddr_in6*)&ssAddr;

		inet_ntop(AF_INET6, &psa6->sin6_addr, cIP, sizeof(cIP));
		snprintf(cOut, sizeof(cOut), "[%s]:%u", cIP, ntohs(psa6->sin6_port));
	}
	else
		return "-";

	return cOut;
}
