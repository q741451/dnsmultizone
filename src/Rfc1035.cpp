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

static unsigned short ReadU16(const std::string &sBuffer, size_t p)
{
	return (unsigned short)(((unsigned char)sBuffer[p] << 8) | (unsigned char)sBuffer[p + 1]);
}

static void WriteU16(std::string &sBuffer, size_t p, unsigned short n)
{
	sBuffer[p] = (char)(n >> 8);
	sBuffer[p + 1] = (char)(n & 0xFF);
}

// 拷贝一个名字；压缩指针按 vRemoved（原偏移, 删掉的字节数）换算成新偏移，
// 指针只往前指，所以被删字节之前的位置不变
static bool CopyName(const std::string &sIn, size_t &p, std::string &sOut, const std::vector<std::pair<size_t, size_t>> &vRemoved)
{
	size_t i = 0;
	size_t t = 0;
	size_t d = 0;

	while (p < sIn.size())
	{
		unsigned char c = (unsigned char)sIn[p];

		if (c == 0)
		{
			sOut += sIn[p++];
			return true;
		}
		if ((c & 0xC0) == 0xC0)
		{
			if (p + 1 >= sIn.size())
				return false;
			t = ReadU16(sIn, p) & 0x3FFF;
			for (i = 0; i < vRemoved.size(); i++)
				if (vRemoved[i].first < t)
					d += vRemoved[i].second;
			t -= d;
			sOut += (char)(0xC0 | (t >> 8));
			sOut += (char)(t & 0xFF);
			p += 2;
			return true;
		}
		if ((c & 0xC0) != 0 || p + 1 + c > sIn.size())
			return false;
		sOut.append(sIn, p, 1 + c);
		p += 1 + c;
	}
	return false;
}

// 问题之后的偏移和回答记录数
bool Rfc1035::QuestionEnd(std::string &sBuffer, size_t *pEnd, unsigned short *uAnswers)
{
	unsigned short uID = 0;
	unsigned short uFlag = 0;
	unsigned short uQType = 0;
	std::string sName;
	AutoBuffer aBuffer;

	aBuffer.Assign((char*)sBuffer.c_str(), sBuffer.size());
	if (ParseRequestAAndAnswers(aBuffer, &uID, &uFlag, uAnswers, sName, &uQType) != true)
		return false;
	*pEnd = aBuffer.m_szOffset;
	return true;
}

// 只留回答部分的前 nAnswers 条；授权、附加两部分去掉
static void SetCounts(std::string &sBuffer, unsigned short nAnswers)
{
	WriteU16(sBuffer, 6, nAnswers);
	WriteU16(sBuffer, 8, 0);
	WriteU16(sBuffer, 10, 0);
}

bool Rfc1035::EmptyResponse(std::string &sBuffer)
{
	unsigned short uAnswers = 0;
	size_t p = 0;

	if (QuestionEnd(sBuffer, &p, &uAnswers) != true)
		return false;
	sBuffer.resize(p);
	SetCounts(sBuffer, 0);
	return true;
}

bool Rfc1035::DropAddress(std::string &sBuffer, unsigned short uQType, bool bDropIPv4, bool bDropIPv6)
{
	std::vector<std::pair<size_t, size_t>> vRemoved;
	std::string sOut;
	std::string sName;
	unsigned short uAnswers = 0;
	unsigned short uType = 0;
	unsigned short uKey = 0;
	unsigned short i = 0;
	size_t p = 0;
	size_t q = 0;
	size_t end = 0;
	size_t lenpos = 0;
	size_t vlen = 0;

	if (uQType == DEF_TYPE_A)
		return bDropIPv4 ? EmptyResponse(sBuffer) : true;
	if (uQType == DEF_TYPE_AAAA)
		return bDropIPv6 ? EmptyResponse(sBuffer) : true;
	if (uQType != DEF_TYPE_SVCB && uQType != DEF_TYPE_HTTPS)
		return true;

	if (QuestionEnd(sBuffer, &p, &uAnswers) != true)
		return false;
	sOut.assign(sBuffer, 0, p);

	for (i = 0; i < uAnswers; i++)
	{
		q = p;
		sName.clear();
		if (CopyName(sBuffer, q, sName, vRemoved) != true || q + 10 > sBuffer.size())
			return false;
		uType = ReadU16(sBuffer, q);
		// 删过字节后，别的类型内部的名字没法不认类型地改指针，到此为止
		if (uType != uQType && vRemoved.size() > 0)
			break;

		CopyName(sBuffer, p, sOut, vRemoved);
		end = p + 10 + ReadU16(sBuffer, p + 8);
		if (end > sBuffer.size())
			return false;
		sOut.append(sBuffer, p, 8);			// TYPE CLASS TTL
		lenpos = sOut.size();
		sOut.append(2, 0);
		p += 10;

		if (uType != uQType)
			sOut.append(sBuffer, p, end - p);
		else
		{
			// SvcPriority、TargetName，然后是一串 key / length / value
			if (p + 2 > end)
				return false;
			sOut.append(sBuffer, p, 2);
			p += 2;
			if (CopyName(sBuffer, p, sOut, vRemoved) != true || p > end)
				return false;
			while (p < end)
			{
				if (p + 4 > end)
					return false;
				uKey = ReadU16(sBuffer, p);
				vlen = 4 + ReadU16(sBuffer, p + 2);
				if (p + vlen > end)
					return false;
				if ((uKey == DEF_SVCPARAM_IPV4HINT && bDropIPv4) || (uKey == DEF_SVCPARAM_IPV6HINT && bDropIPv6))
					vRemoved.push_back(std::make_pair(p, vlen));
				else
					sOut.append(sBuffer, p, vlen);
				p += vlen;
			}
		}

		WriteU16(sOut, lenpos, (unsigned short)(sOut.size() - lenpos - 2));
		p = end;
	}

	// 附加部分常带目标的 A / AAAA，一并去掉
	SetCounts(sOut, i);
	sBuffer = sOut;
	return true;
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
