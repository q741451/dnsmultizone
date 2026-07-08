#include "stdafx.h"

bool Rfc1035::ParseRequestA(std::string &sBuffer, unsigned short *uID, unsigned short *uFlag, std::string &sName, bool *bIsA)
{
	bool ret = false;
	unsigned short uAnswers = 0;
	AutoBuffer aBuffer;

	aBuffer.Assign((char*)sBuffer.c_str(), sBuffer.size());

	if (ParseRequestAAndAnswers(aBuffer, uID, uFlag, &uAnswers, sName, bIsA) != true)
		goto end;

	ret = true;
end:
	return ret;
}

bool Rfc1035::ParseResponseA(std::string &sBuffer, unsigned short *uID, unsigned short *uFlag, std::string &sName, bool *bIsA, std::list<unsigned int> &luIPs)
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

	if (ParseRequestAAndAnswers(aBuffer, uID, uFlag, &uAnswers, sName, bIsA) != true)
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
			case 0x0005: // CNAME
				break;
			case 0x0001: // A
				if(sData.size() == sizeof(unsigned int))
					luIPs.push_back(ntohl(*(unsigned int*)sData.c_str()));
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

bool Rfc1035::ParseRequestAAndAnswers(AutoBuffer &aBuffer, unsigned short *uID, unsigned short *uFlag, unsigned short *uAnswers, std::string &sName, bool *bIsA)
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

	if (uType == 0x0001)
		*bIsA = true;
	else
		*bIsA = false;

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
			ret = true; // ·ûºÏÐèÇó
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
