#ifndef _DNS_QUERY_WORK_ITEM_H
#define _DNS_QUERY_WORK_ITEM_H

class DNSQueryResultItem
{
public:
	enum EnumState
	{
		ENUM_STATE_WAIT,
		ENUM_STATE_ERROR,
		ENUM_STATE_MATCH,
		ENUM_STATE_NOT_MATCH,
	};
	DNSQueryResultItem()
	{
		m_eState = ENUM_STATE_WAIT;
	}
	EnumState m_eState;
	std::list<unsigned int> m_luIPs;
	std::string m_sDNSData;
};

class DNSQureyWorkItem
{
public:
	DNSQureyWorkItem()
	{
		m_nID = 0;
		m_nMaxDoneIndex = 0;
		m_bIsA = true;
		m_bIsDone = false;
	}
	unsigned short	m_nID;
	unsigned int	m_nMaxDoneIndex;
	bool			m_bIsA;
	bool			m_bIsDone;
	std::vector<std::shared_ptr<DNSQueryResultItem>> m_vsDNSQueryResultItems;
};



#endif
