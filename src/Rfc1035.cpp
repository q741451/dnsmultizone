#include "stdafx.h"

bool Rfc1035::IsIPBearingType(unsigned short uQType)
{
	return (uQType == DEF_TYPE_A || uQType == DEF_TYPE_AAAA ||
		uQType == DEF_TYPE_SVCB || uQType == DEF_TYPE_HTTPS);
}

bool Rfc1035::ParseRequestA(std::string &sBuffer, unsigned short *uID, unsigned short *uFlag, std::string &sName, unsigned short *uQType)
{
	bool ret = false;
	unsigned short uAnswers = 0;
	AutoBuffer aBuffer;

	aBuffer.Assign((char*)sBuffer.c_str(), sBuffer.size());

	if (ParseRequestAAndAnswers(aBuffer, uID, uFlag, &uAnswers, sName, uQType) != true)
		goto end;

	ret = true;
end:
	return ret;
}

bool Rfc1035::ParseResponseA(std::string &sBuffer, unsigned short *uID, unsigned short *uFlag, std::string &sName, unsigned short *uQType,
	std::list<unsigned int> &luIPs, std::list<IPv6Addr> &luIP6s)
{
	bool ret = false;
	int i = 0;
	unsigned short uAnswers = 0;
	unsigned short uType = 0;
	unsigned short uClass = 0;
	unsigned int uTimeToLive = 0;
	unsigned short uDataLength = 0;
	std::string sData;
	AutoBuffer aBuffer;

	aBuffer.Assign((char*)sBuffer.c_str(), sBuffer.size());

	if (ParseRequestAAndAnswers(aBuffer, uID, uFlag, &uAnswers, sName, uQType) != true)
		goto end;

	for (i = 0; i < uAnswers; i++)
	{
		if (SkipBufferName(aBuffer) != true)
			goto end;

		if (aBuffer.ReadUINT16(&uType) != true)
			goto end;

		if (aBuffer.ReadUINT16(&uClass) != true)
			goto end;

		if (uClass != 0x0001)
			goto end;

		if (aBuffer.ReadUINT32(&uTimeToLive) != true)
			goto end;

		if (aBuffer.ReadUINT16(&uDataLength) != true)
			goto end;

		if (uDataLength)
		{
			sData.resize(uDataLength);
			if (aBuffer.ReadBuffer((char*)sData.c_str(), uDataLength) != true)
				goto end;

			switch (uType)
			{
			case DEF_TYPE_CNAME: // CNAME
				break;
			case DEF_TYPE_A: // A
				if (sData.size() == sizeof(unsigned int))
				{
					unsigned int uIP = 0;
					memcpy(&uIP, sData.c_str(), sizeof(uIP));
					luIPs.push_back(ntohl(uIP));
				}
				break;
			case DEF_TYPE_AAAA: // AAAA
				if (sData.size() == sizeof(IPv6Addr))
				{
					IPv6Addr iaAddr;
					memcpy(iaAddr.m_cAddr, sData.c_str(), sizeof(iaAddr.m_cAddr));
					luIP6s.push_back(iaAddr);
				}
				break;
			case DEF_TYPE_SVCB: // SVCB
			case DEF_TYPE_HTTPS: // HTTPS
				// hint 是客户端可能直接拿去建连的地址，
				// 因此和 A/AAAA 记录一样要过 ipList 校验
				if (ParseSvcParamHint(sData, luIPs, luIP6s) != true)
					goto end;
				break;
			default:
				goto end;
			}
		}
	}

	ret = true;
end:
	return ret;
}

bool Rfc1035::ParseSvcParamHint(std::string &sData, std::list<unsigned int> &luIPs, std::list<IPv6Addr> &luIP6s)
{
	bool ret = false;
	unsigned short uPriority = 0;
	unsigned short uKey = 0;
	unsigned short uLen = 0;
	std::string sValue;
	AutoBuffer aBuffer;

	aBuffer.Assign((char*)sData.c_str(), sData.size());

	// RDATA 结构：SvcPriority + TargetName + 若干 SvcParam，见 RFC 9460
	if (aBuffer.ReadUINT16(&uPriority) != true)
		goto end;

	if (SkipBufferName(aBuffer) != true)
		goto end;

	while (aBuffer.GetSize() - aBuffer.m_szOffset >= sizeof(unsigned short) * 2)
	{
		if (aBuffer.ReadUINT16(&uKey) != true)
			goto end;

		if (aBuffer.ReadUINT16(&uLen) != true)
			goto end;

		sValue.resize(uLen);
		if (uLen && aBuffer.ReadBuffer((char*)sValue.c_str(), uLen) != true)
			goto end;

		if (uKey == DEF_SVCPARAM_IPV4HINT)
		{
			// Assign 不会重置偏移，每个参数都要用新的读取器
			AutoBuffer aHint;
			unsigned int uIP = 0;

			aHint.Assign((char*)sValue.c_str(), sValue.size());
			while (aHint.ReadUINT32(&uIP) == true)
				luIPs.push_back(uIP);
		}
		else if (uKey == DEF_SVCPARAM_IPV6HINT)
		{
			size_t szPos = 0;

			for (szPos = 0; szPos + sizeof(IPv6Addr) <= sValue.size(); szPos += sizeof(IPv6Addr))
			{
				IPv6Addr iaAddr;
				memcpy(iaAddr.m_cAddr, sValue.c_str() + szPos, sizeof(iaAddr.m_cAddr));
				luIP6s.push_back(iaAddr);
			}
		}
	}

	ret = true;
end:
	return ret;
}

bool Rfc1035::ParseRequestAAndAnswers(AutoBuffer &aBuffer, unsigned short *uID, unsigned short *uFlag, unsigned short *uAnswers, std::string &sName, unsigned short *uQType)
{
	bool ret = false;
	unsigned short uQuestions = 0;
	unsigned short uShortNumber = 0;
	unsigned short uType = 0;
	unsigned short uClass = 0;

	if (aBuffer.ReadUINT16(uID) != true)
		goto end;

	if (aBuffer.ReadUINT16(uFlag) != true)
		goto end;

	if (aBuffer.ReadUINT16(&uQuestions) != true)
		goto end;

	if (uQuestions != 1)
		goto end;

	// Answer RRs
	if (aBuffer.ReadUINT16(uAnswers) != true)
		goto end;

	// Authority RRs
	if (aBuffer.ReadUINT16(&uShortNumber) != true)
		goto end;

	// Additional RRs
	if (aBuffer.ReadUINT16(&uShortNumber) != true)
		goto end;

	if (GetBufferName(aBuffer, sName) != true)
		goto end;

	if (aBuffer.ReadUINT16(&uType) != true)
		goto end;

	*uQType = uType;

	if (aBuffer.ReadUINT16(&uClass) != true)
		goto end;

	if (uClass != 0x0001)
		goto end;

	ret = true;
end:
	return ret;
}

bool Rfc1035::GetBufferName(AutoBuffer &aBuffer, std::string &sName)
{
	bool ret = false;
	std::string sPartName;
	unsigned char cNextLen = 0;

	sName.clear();
	while (1)
	{
		if (aBuffer.ReadUINT8(&cNextLen) != true)
			goto end;

		if ((cNextLen & 0xC0) == 0xC0)
		{
			if (aBuffer.ReadUINT8(&cNextLen) != true)
				goto end;
			goto end;
		}

		if(cNextLen == 0)
			break;

		sPartName.resize(cNextLen);
		aBuffer.ReadBuffer((char*)sPartName.c_str(), cNextLen);

		sName += (sPartName + ".");
	}

	if(sName.size() > 0)
		sName.resize(sName.size() - 1);

	ret = true;
end:
	return ret;
}

bool Rfc1035::SkipBufferName(AutoBuffer &aBuffer)
{
	bool ret = false;
	std::string sName;
	std::string sPartName;
	unsigned char cNextLen = 0;

	while (1)
	{
		if (aBuffer.ReadUINT8(&cNextLen) != true)
			goto end;

		if ((cNextLen & 0xC0) == 0xC0)
		{
			if (aBuffer.ReadUINT8(&cNextLen) != true)
				goto end;
			ret = true; // 符合需求
			goto end;
		}

		if (cNextLen == 0)
			break;

		sPartName.resize(cNextLen);
		aBuffer.ReadBuffer((char*)sPartName.c_str(), cNextLen);
	}

	ret = true;
end:
	return ret;
}
