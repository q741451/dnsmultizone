#ifndef _AUTO_BUFFER_H
#define _AUTO_BUFFER_H


class AutoBuffer
{
public:
	AutoBuffer()
	{
		m_szOffset = 0;
	}

	void Assign(void *pData, size_t szLen)
	{
		m_sBuffer.assign((char*)pData, szLen);
	}

	bool SetOffset(size_t szOffset)
	{
		if (szOffset >= m_sBuffer.size())
			return false;
		m_szOffset = szOffset;
		return true;
	}


	bool ReadUINT32(unsigned int *pNumber)
	{
		if (m_sBuffer.size() - m_szOffset < sizeof(unsigned int))
			return false;

		*pNumber = ntohl(*(unsigned int*)GetCurPtr());

		m_szOffset += sizeof(unsigned int);

		return true;
	}

	bool ReadUINT16(unsigned short *pNumber)
	{
		if (m_sBuffer.size() - m_szOffset < sizeof(unsigned short))
			return false;

		*pNumber = ntohs(*(unsigned short*)GetCurPtr());

		m_szOffset += sizeof(unsigned short);

		return true;
	}

	bool ReadUINT8(unsigned char *pNumber)
	{
		if (m_sBuffer.size() - m_szOffset < sizeof(unsigned char))
			return false;

		*pNumber = *(unsigned char*)GetCurPtr();

		m_szOffset += sizeof(unsigned char);

		return true;
	}

	bool ReadBuffer(void *pDist, size_t szLen)
	{
		if (m_sBuffer.size() - m_szOffset < szLen)
			return false;

		memcpy(pDist, GetCurPtr(), szLen);

		m_szOffset += szLen;

		return true;
	}

	void PushUINT32(unsigned int nNumber)
	{
		nNumber = htonl(nNumber);
		m_sBuffer.append((char*)&nNumber, sizeof(nNumber));
	}

	void PushUINT16(unsigned short nNumber)
	{
		nNumber = htons(nNumber);
		m_sBuffer.append((char*)&nNumber, sizeof(nNumber));
	}

	void AppendBuffer(void *pDist, size_t szLen)
	{
		m_sBuffer.append((char*)pDist, szLen);
	}

	void *GetPtr()
	{
		return (void*)m_sBuffer.c_str();
	}

	void *GetCurPtr()
	{
		return (void*)((char*)GetPtr() + m_szOffset);
	}

	size_t GetSize()
	{
		return m_sBuffer.size();
	}


	std::string m_sBuffer;		// 继续偷懒用string存数据
	size_t		m_szOffset;
};


#endif

